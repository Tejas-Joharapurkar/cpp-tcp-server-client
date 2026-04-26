#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <cstring>
#include "Message.hxx"

int createSocketServer(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        return -1;
    }

    listen(server_fd, 10);
    std::cout << "Server listening on port " << port << "\n";
    return server_fd;
}

struct ClientContext
{
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<Response> q;
};

std::unordered_map<int, ClientContext *> mp;

void readThread(int fd)
{
    ClientContext *ctx = mp[fd];

    while (true)
    {
        // Lock ke BAHAR padhna — blocking call hai
        char buffer[1024] = {};
        int bytes = read(fd, buffer, sizeof(buffer));
        if (bytes <= 0)
        {
            std::cout << "Client disconnected\n";
            break;
        }

        Message message{};
        decode(buffer, message);

        std::cout << "[NewOrder]"
                  << " Symbol: " << message.order.symbol
                  << " Price: " << message.order.price
                  << " Qty: " << message.order.quantity
                  << " Side: " << (message.order.side == 1 ? "BUY" : "SELL")
                  << "\n";

        // Response banao
        Response res{};
        res.header.networkID = message.header.networkID;
        std::string ack = "Order acknowledged: ";
        ack += message.order.symbol;
        strncpy(res.res, ack.c_str(), sizeof(res.res) - 1);

        // Sirf queue push ke liye lock lo
        {
            std::unique_lock<std::mutex> lock(ctx->mtx);
            ctx->cv.wait(lock, [&]
                         { return ctx->q.size() < 11; });
            ctx->q.push(res);
        }
        ctx->cv.notify_all();
    }
}

void writeThread(int fd)
{
    ClientContext *ctx = mp[fd];

    while (true)
    {
        Response res;

        // Queue se nikalo
        {
            std::unique_lock<std::mutex> lock(ctx->mtx);
            ctx->cv.wait(lock, [&]
                         { return !ctx->q.empty(); });
            res = ctx->q.front();
            ctx->q.pop(); // pop karna zaroori!
        }
        ctx->cv.notify_all();

        // Lock ke BAHAR bhejo
        char buffer[1024] = {};
        int len = encode(res, buffer, sizeof(buffer));
        if (len <= 0)
            continue;

        int totalSent = 0;
        while (totalSent < len)
        {
            int sent = send(fd, buffer + totalSent, len - totalSent, 0);
            if (sent <= 0)
            {
                perror("send failed");
                return;
            }
            totalSent += sent;
        }
    }
}

void acceptLoop()
{
    int serverFD = createSocketServer(3000);
    if (serverFD < 0)
        return;

    std::vector<std::thread> threads;

    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(serverFD, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept failed");
            continue;
        }

        std::cout << "Client connected! port=" << ntohs(client_addr.sin_port) << "\n";

        // Pehle context banao, phir threads
        ClientContext *ctx = new ClientContext();
        mp[client_fd] = ctx;

        // vector mein rakho — detach nahi
        threads.push_back(std::thread(readThread, client_fd));
        threads.push_back(std::thread(writeThread, client_fd));

        // Dono ko detach karo taaki accept block na ho
        threads[threads.size() - 2].detach();
        threads[threads.size() - 1].detach();
    }
}

int main()
{
    std::thread acc(acceptLoop);
    acc.join();
    return 0;
}