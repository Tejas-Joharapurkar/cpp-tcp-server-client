#pragma once
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "ClientContext.hxx"

class ThreadPool
{
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<ClientContext *> q;
    bool stop = false;
    std::vector<std::thread> threads;

public:
    ThreadPool() = default;
    ~ThreadPool();
    void init(int noOfWorkers);
    void shutdown();
    void pushTask(ClientContext *ctx);

private:
    void workerLoop();
};