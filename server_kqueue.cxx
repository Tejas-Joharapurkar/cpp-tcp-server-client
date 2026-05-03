#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <iostream>
#include <cerrno>
// #include "ClientContext.hxx"
#include "ThreadPool.hxx"

void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void registerFD(int kq, int fd, void *udata)
{
    struct kevent change;
    EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, udata);
    kevent(kq, &change, 1, nullptr, 0, nullptr);
}

void unregisterFD(int kq, int fd)
{
    struct kevent change;
    EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kq, &change, 1, nullptr, 0, nullptr);
}

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

int main()
{
    int kq = kqueue();
    int server_fd = createSocketServer(12345);
    if (server_fd < 0)
        return 1;

    ThreadPool tp;
    tp.init(4);

    setNonBlocking(server_fd);
    registerFD(kq, server_fd, nullptr);

    struct kevent events[64];

    while (true)
    {
        int n = kevent(kq, nullptr, 0, events, 64, nullptr);
        if (n < 0)
        {
            perror("kevent failed");
            break;
        }

        for (int i = 0; i < n; i++)
        {

            // new connection
            if ((int)events[i].ident == server_fd)
            {
                while (true)
                {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);

                    int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);

                    if (client_fd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // sab accept ho gaye
                        perror("accept failed");
                        break;
                    }

                    setNonBlocking(client_fd);
                    ClientContext *ctx = new ClientContext(client_fd);
                    registerFD(kq, client_fd, (void *)ctx);
                    std::cout << "Client connected! fd=" << client_fd << "\n";
                }
            }

            // client data ready
            else
            {
                ClientContext *ctx = (ClientContext *)events[i].udata;

                // client disconnect
                if (events[i].flags & EV_EOF)
                {
                    std::cout << "Client disconnected fd=" << events[i].ident << "\n";
                    unregisterFD(kq, events[i].ident);
                    delete ctx;
                    continue;
                }

                tp.pushTask(ctx);
            }
        }
    }

    tp.shutdown();
    close(server_fd);
    close(kq);
    return 0;
}