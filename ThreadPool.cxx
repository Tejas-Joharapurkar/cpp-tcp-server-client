#include "ThreadPool.hxx"
#include <iostream>
#include "Message.hxx"

void ThreadPool::init(int noOfWorkers)
{
    for (int i = 0; i < noOfWorkers; i++)
    {
        threads.push_back(std::thread(&ThreadPool::workerLoop, this));
    }
}

void ThreadPool::pushTask(ClientContext *ctx)
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        q.push(ctx);
    }
    cv.notify_one(); // sirf ek worker jagao — sab nahi
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        ClientContext *ctx = nullptr;

        // task lo
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]
                    { return !q.empty() || stop; });

            if (stop && q.empty())
                break;

            ctx = q.front();
            q.pop();
        } // lock yahan release — process karo bina lock ke

        // message padho
        int status = ctx->handler.Read();
        if (status <= 0)
        {
            std::cout << "Client disconnected\n";
            ctx->done = true;
            delete ctx;
            continue;
        }

        // decode + dispatch
        char *raw = ctx->handler.getMessage();
        const Header *hdr = reinterpret_cast<const Header *>(raw);

        switch (hdr->templateID)
        {
        case MSG_ORDER:
        {
            Message msg{};
            decode(raw, msg);

            std::cout << "[Order] Symbol: " << msg.order.symbol
                      << " Price: " << msg.order.price
                      << " Qty: " << msg.order.quantity
                      << " Side: " << (msg.order.side == 1 ? "BUY" : "SELL")
                      << "\n";

            // response banao
            Response res{};
            res.header.networkID = msg.header.networkID;
            std::string ack = "Order acknowledged: ";
            ack += msg.order.symbol;
            strncpy(res.res, ack.c_str(), sizeof(res.res) - 1);

            // encode + send
            char buf[1024] = {};
            int len = encode(res, buf, sizeof(buf));
            ctx->handler.Write(buf, len);
            break;
        }
        default:
            std::cout << "Unknown templateID: " << hdr->templateID << "\n";
        }

        ctx->handler.consumeMessage();
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }
}