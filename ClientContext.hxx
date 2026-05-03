#include <queue>
#include <unordered_map>
#include <mutex>
#include "Message.hxx"
#include "ProtocolHndler.hxx"
struct ClientContext
{
    int fd;
    ProtocolHndler handler; // read buffer + partial read handling
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<Response> writeQ;
    bool done = false;

    ClientContext(int fd) : fd(fd), handler(fd) {} // handler ko fd pass karo
};