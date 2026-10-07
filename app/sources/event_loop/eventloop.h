#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include <atomic>
#include <functional>
#include <map>

enum EVENT_LOOP_SIGNALS
{
    SIG_NONE = 0,
    SIG_EXIT,
};

class EventLoop
{
  public:
    EventLoop(uint32_t events, int fd);
    ~EventLoop();

    void start();
    bool initEventPoll();

    bool bindSlot(uint32_t sig, std::function< EVENT_LOOP_SIGNALS() > func);
    bool reBindSlot(uint32_t sig, std::function< EVENT_LOOP_SIGNALS() > func);
    void breakEventLoop();

  private:
    bool register_fd();
    bool unregister_fd();
    bool processEventLoop();

  private:
    int                                                         fd_ { -1 };
    int                                                         epollFd_ { -1 };
    uint32_t                                                    eventsMask_ { 0 };
    std::atomic_bool                                            stop_ { false };
    std::atomic_bool                                            running_ { false };
    std::map< uint32_t, std::function< EVENT_LOOP_SIGNALS() > > slots_;
};

#endif  // EVENTLOOP_H
