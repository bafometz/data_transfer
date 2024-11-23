#include "eventloop.h"
#include "../logger/logger.h"
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

EventLoop::EventLoop(uint32_t events, int fd) :
    fd_ { fd },
    eventsMask_ { events }
{
}

EventLoop::~EventLoop()
{
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd_, NULL);
    ::close(epollFd_);
}

std::array< uint32_t, 15 > EventLoop::eventsArray_ { EPOLLIN,  EPOLLPRI, EPOLLOUT,   EPOLLRDNORM,    EPOLLRDBAND, EPOLLWRNORM,  EPOLLWRBAND, EPOLLMSG,
                                                     EPOLLERR, EPOLLHUP, EPOLLRDHUP, EPOLLEXCLUSIVE, EPOLLWAKEUP, EPOLLONESHOT, EPOLLET };

void EventLoop::start()
{
    running_ = true;
    for (;;)
    {
        if (stop_) break;
        if (!processEventLoop())
        {
            break;
        }
    }

    running_ = false;
}

bool EventLoop::initEventPoll()
{
    epollFd_ = epoll_create(maxEvents_);

    if (epollFd_ < 0)
    {
        LOG_ERROR("Can't create epoll", std::strerror(errno));
        return false;
    }

    if (!register_fd())
    {
        return false;
    }

    return true;
}

bool EventLoop::bindSlot(uint32_t sig, std::function< EVENT_LOOP_SIGNALS() > func)
{
    auto find = slots_.find(sig);
    if (find == slots_.end())
    {
        slots_.insert({ sig, func });
        return true;
    }

    return false;
}

bool EventLoop::reBindSlot(uint32_t sig, std::function< EVENT_LOOP_SIGNALS() > func)
{
    auto find = slots_.find(sig);
    if (find != slots_.end())
    {
        slots_[sig] = func;
        return true;
    }

    return false;
}

void EventLoop::breakEventLoop()
{
    stop_ = true;
}

bool EventLoop::register_fd()
{
    struct epoll_event ev;
    ev.events  = eventsMask_;
    ev.data.fd = fd_;
    auto res   = epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd_, &ev);

    if (res == -1)
    {
        LOG_ERROR("Can't assign file descriptor to epoll", std::strerror(errno));
        return false;
    }

    return true;
}

bool EventLoop::unregister_fd()
{
    auto res = epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd_, NULL);

    if (res == -1)
    {
        LOG_ERROR("Can't remove file descriptor from epoll", std::strerror(errno));
        return false;
    }

    return true;
}

bool EventLoop::processEventLoop()
{
    EPOLLIN;
    struct epoll_event events[maxEvents_];
    auto               newEventsCount = epoll_wait(epollFd_, events, maxEvents_, timeout_);
    auto               errmask        = EPOLLERR | EPOLLHUP;

    if (newEventsCount < 0 && errno == EINTR) return false;

    if (newEventsCount == 0)
    {
        // LOG_INFO("Timeout for event loop reached", timeout_, "msec");
        return true;
    }

    for (int i = 0; i < newEventsCount; i++)
    {
        auto event { events[i] };

        if (event.events & errmask)
        {
            LOG_ERROR("Detected error, exit");
            return false;
        }

        for (const auto& eventVal : eventsArray_)
        {
            if (!static_cast< bool >(eventVal & event.events)) continue;

            auto findSlot = slots_.find(eventVal);

            if (findSlot != slots_.end())
            {
                auto returnSatus = findSlot->second();
                if (returnSatus == EVENT_LOOP_SIGNALS::SIG_EXIT) return false;
            }
            else
            {
                // LOG_WARN("Recived signal", eventVal, "but slot not found");
            }
        }
    }

    return true;
}
