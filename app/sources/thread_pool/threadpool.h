#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

class Server;

using SocketTask = std::function< void() >;

class ThreadPool
{
  public:
    ThreadPool(size_t threadNumber);
    ~ThreadPool();

    template< class F, class... Args >
    auto enqueue(F&& f, Args&&... args) -> std::future< typename std::result_of< F(Args...) >::type >;

  private:
    std::vector< std::thread > m_workers;
    std::queue< SocketTask >   m_tasks;

    std::mutex              m_mutex;
    std::condition_variable m_condition;
    bool                    m_stop;
};

inline ThreadPool::ThreadPool(size_t threadNumber) :
    m_stop(false)
{
    for (size_t i = 0; i < threadNumber; ++i)
    {
        m_workers.emplace_back(
            [this]
            {
                for (;;)
                {
                    SocketTask task;

                    {
                        std::unique_lock< std::mutex > lock(this->m_mutex);
                        this->m_condition.wait(lock, [this] { return this->m_stop || !this->m_tasks.empty(); });

                        if (this->m_stop && this->m_tasks.empty())
                        {
                            return;
                        }

                        task = std::move(this->m_tasks.front());
                        this->m_tasks.pop();
                    }

                    task();
                }
            });
    }
}

template< class F, class... Args >
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future< typename std::result_of< F(Args...) >::type >
{
    using return_type = typename std::invoke_result< F, Args... >::type;
    if (m_stop)
    {
        //        return false;
        throw std::runtime_error("enqueue on stopped ThreadPool");
        //        return std::future<return_type>;
        //        return std::future<bool>(false);
    }

    auto task = std::make_shared< std::packaged_task< return_type() > >(std::bind(std::forward< F >(f), std::forward< Args >(args)...));

    std::future< return_type > res = task->get_future();
    {
        std::unique_lock< std::mutex > lock(m_mutex);

        m_tasks.emplace([task]() { (*task)(); });
    }
    m_condition.notify_one();
    return res;
}

inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock< std::mutex > lock(m_mutex);
        m_stop = true;
    }

    m_condition.notify_all();

    for (std::thread& worker : m_workers)
    {
        worker.join();
    }
}

#endif  // THREADPOOL_H
