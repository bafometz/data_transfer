#ifndef SERVER_H
#define SERVER_H
#include "../event_loop/eventloop.h"
#include "../file_send_state/transmittionStatus.h"
#include "../socket/socket.h"
#include "../thread_pool/threadpool.h"

#include <atomic>
#include <csignal>
#include <map>

struct SignalHandler
{
    static SignalHandler& instance()
    {
        static SignalHandler s;
        return s;
    }  // instance

    SignalHandler(const SignalHandler&)            = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

    void setAtomic(std::atomic_bool* pAtomic) { exit = pAtomic; }

    void disableAtomic() { exit = nullptr; };

    static void signalHandler(int signum)
    {
        if (signum == SIGPIPE) return;

        if (exit && !(*exit))
        {
            if (SIGINT) *exit = true;
            if (SIGKILL) *exit = true;
            if (SIGTERM) *exit = true;
            if (SIGABRT) *exit = true;
        }
    }

  private:
    inline static std::atomic_bool* exit = nullptr;

    SignalHandler() {}

    ~SignalHandler() {}

};  // struct SignalHandler

class Server
{
    using locker = std::lock_guard< std::mutex >;

  public:
    Server(int port);
    ~Server();
    int start();

  private:
    bool openConnection();

    SocketPtr acceptNewConnection();
    void      createSubEventLoop(SocketPtr);

    void recivePackage(EventLoop& ev, transmit_state& state, SocketPtr pSock);
    void sendPackage(EventLoop& ev, transmit_state& state, SocketPtr pSock);

  private:
    int epollFd_ = -1;
    int port_    = 7021;

    const int maxEventsConnectionToHandle_ = std::thread::hardware_concurrency();

    SocketPtr  masterSocket_ = nullptr;
    ThreadPool tp { static_cast< size_t >(maxEventsConnectionToHandle_) };
};

#endif  // SERVER_H
