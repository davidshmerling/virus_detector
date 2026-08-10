#include "ThreadPool/ThreadPool.h"

#include <utility>

namespace {

// Fixed pool size: enough parallelism for I/O-bound file scans without
// oversubscribing a typical laptop/server.
constexpr std::size_t kDefaultWorkers = 16;

// Bounded queue so discovery cannot race ahead of workers unboundedly.
constexpr std::size_t kDefaultQueueCapacity = 1024;

}  // namespace

ThreadPool::ThreadPool()
{
    queue_capacity_ = kDefaultQueueCapacity;
    workers_.reserve(kDefaultWorkers);

    for (std::size_t i = 0; i < kDefaultWorkers; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool()
{
    // Signal stop, wake waiters, and join so no worker outlives *this.
    {
        std::scoped_lock lock(mutex_);
        stop_ = true;
    }

    task_available_cv_.notify_all();
    queue_space_cv_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool ThreadPool::enqueue(std::function<void()> task)
{
    std::unique_lock lock(mutex_);

    queue_space_cv_.wait(lock, [this]() {
        return stop_ || tasks_.size() < queue_capacity_;
    });

    if (stop_) {
        return false;
    }

    tasks_.push_back(std::move(task));
    lock.unlock();
    task_available_cv_.notify_one();
    return true;
}

void ThreadPool::wait()
{
    std::unique_lock lock(mutex_);
    all_tasks_done_cv_.wait(lock, [this]() {
        return tasks_.empty() && active_tasks_ == 0;
    });
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock lock(mutex_);
            task_available_cv_.wait(lock, [this]() {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
            ++active_tasks_;
        }

        queue_space_cv_.notify_one();

        try {
            task();
        } catch (...) {
            // Swallow so one failing task cannot kill the worker thread; the
            // scan continues and wait() can still complete.
        }

        {
            std::scoped_lock lock(mutex_);
            --active_tasks_;
            if (tasks_.empty() && active_tasks_ == 0) {
                all_tasks_done_cv_.notify_all();
            }
        }
    }
}
