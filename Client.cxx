#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include "Message.hxx"

// ---- Logger ----
std::mutex logMtx;

std::string timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%H:%M:%S");
    return ss.str();
}

void log(int clientID, const std::string &tag, const std::string &msg)
{
    std::lock_guard<std::mutex> lock(logMtx);
    std::cout << "[" << timestamp() << "]"
              << " [Client-" << clientID << "]"
              << " [" << tag << "] "
              << msg << "\n";
}

// ---- Per connection state ----
struct ConnectionState
{
    int fd;
    int clientID;
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<Message> sendQ;
    bool done = false;
};

// ---- Send thread ----
void sendThread(ConnectionState &state)
{
    log(state.clientID, "SEND", "Send thread started");

    while (true)
    {
        Message msg;

        {
            std::unique_lock<std::mutex> lock(state.mtx);
            state.cv.wait(lock, [&]
                          { return !state.sendQ.empty() || state.done; });

            if (state.done && state.sendQ.empty())
            {
                log(state.clientID, "SEND", "Queue empty, shutting down");
                break;
            }

            msg = state.sendQ.front();
            state.sendQ.pop();
        }
        state.cv.notify_all();

        char buf[1024] = {};
        int len = encode(msg, buf, sizeof(buf));
        if (len <= 0)
        {
            log(state.clientID, "SEND", "Encode failed, skipping");
            continue;
        }

        int totalSent = 0;
        while (totalSent < len)
        {
            int sent = send(state.fd, buf + totalSent, len - totalSent, 0);
            if (sent <= 0)
            {
                log(state.clientID, "SEND", "Send failed — server disconnected?");
                state.done = true;
                state.cv.notify_all();
                return;
            }
            totalSent += sent;
        }

        log(state.clientID, "SEND",
            std::string("Order sent: ") + msg.order.symbol +
                " price=" + std::to_string(msg.order.price) +
                " qty=" + std::to_string(msg.order.quantity) +
                " side=" + (msg.order.side == 1 ? "BUY" : "SELL"));
    }
}

// ---- Recv thread ----
void recvThread(ConnectionState &state)
{
    log(state.clientID, "RECV", "Recv thread started");

    while (true)
    {
        char buf[1024] = {};
        int bytes = recv(state.fd, buf, sizeof(buf), 0);

        if (bytes <= 0)
        {
            log(state.clientID, "RECV", "Server disconnected");
            state.done = true;
            state.cv.notify_all();
            break;
        }

        Header hdr{};
        memcpy(&hdr, buf, sizeof(Header));

        switch (hdr.templateID)
        {
        case MSG_RESPONSE:
        {
            Response res{};
            decode(buf, res);
            log(state.clientID, "RECV", std::string("Server response: ") + res.res);
            break;
        }
        default:
            log(state.clientID, "RECV", "Unknown templateID: " + std::to_string(hdr.templateID));
        }
    }
}

// ---- One connection ----
void runConnection(int clientID, const std::string *symbols, const double *prices, int orderCount)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        log(clientID, "MAIN", "socket failed");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(3000);

    if (connect(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log(clientID, "MAIN", "connect failed");
        close(fd);
        return;
    }
    log(clientID, "MAIN", "Connected to server!");

    ConnectionState state;
    state.fd = fd;
    state.clientID = clientID;

    std::thread sender(sendThread, std::ref(state));
    std::thread receiver(recvThread, std::ref(state));

    // Orders queue mein daalo
    for (int i = 0; i < orderCount; i++)
    {
        Message msg{};
        msg.header.networkID = clientID;
        msg.order.price = prices[i];
        msg.order.quantity = (i + 1) * 100;
        msg.order.side = (i % 2 == 0) ? 1 : 2;
        strncpy(msg.order.symbol, symbols[i].c_str(), sizeof(msg.order.symbol));

        {
            std::unique_lock<std::mutex> lock(state.mtx);
            state.sendQ.push(msg);
        }
        state.cv.notify_all();

        log(clientID, "MAIN", std::string("Queued: ") + symbols[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Done mark karo
    {
        std::unique_lock<std::mutex> lock(state.mtx);
        state.done = true;
    }
    state.cv.notify_all();

    sender.join();

    shutdown(fd, SHUT_RDWR);
    receiver.join();

    close(fd);
    log(clientID, "MAIN", "Connection closed");
}

int main()
{
    std::string symbols1[] = {"RELIANCE", "TATASTEEL", "INFY"};
    double prices1[] = {450.50, 120.75, 1800.00};

    std::string symbols2[] = {"HDFC", "WIPRO", "ONGC"};
    double prices2[] = {1650.25, 400.00, 210.50};

    std::string symbols3[] = {"BAJAJ", "MARUTI", "SUNPHARMA"};
    double prices3[] = {3200.00, 9500.00, 890.00};

    // 3 connections simultaneously
    std::thread c1(runConnection, 1, symbols1, prices1, 3);
    std::thread c2(runConnection, 2, symbols2, prices2, 3);
    std::thread c3(runConnection, 3, symbols3, prices3, 3);

    c1.join();
    c2.join();
    c3.join();

    std::cout << "\nAll clients done!\n";
    return 0;
}