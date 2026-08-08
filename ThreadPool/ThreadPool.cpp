#include "ThreadPool/ThreadPool.h"

#include <utility>

namespace {

constexpr std::size_t kDefaultWorkers = 16;
constexpr std::size_t kDefaultQueueCapacity = 1024;

}  // namespace

ThreadPool::ThreadPool(std::size_t worker_count, std::size_t queue_capacity)
{
    if (worker_count == 0) {
        worker_count = kDefaultWorkers;
    }
    if (queue_capacity == 0) {
        queue_capacity = kDefaultQueueCapacity;
    }

    queue_capacity_ = queue_capacity;
    workers_.reserve(worker_count);

    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::scoped_lock lock(mutex_);
        stop_ = true;
    }

    task_cv_.notify_all();
    space_cv_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool ThreadPool::enqueue(std::function<void()> task)
{
    std::unique_lock lock(mutex_);

    space_cv_.wait(lock, [this]() {
        return stop_ || tasks_.size() < queue_capacity_;
    });

    if (stop_) {
        return false;
    }

    tasks_.push_back(std::move(task));
    lock.unlock();
    task_cv_.notify_one();
    return true;
}

void ThreadPool::wait()
{
    std::unique_lock lock(mutex_);
    idle_cv_.wait(lock, [this]() {
        return tasks_.empty() && active_tasks_ == 0;
    });
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock lock(mutex_);
            task_cv_.wait(lock, [this]() {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
            ++active_tasks_;
        }

        space_cv_.notify_one();

        try {
            task();
        } catch (...) {
        }

        {
            std::scoped_lock lock(mutex_);
            --active_tasks_;
            if (tasks_.empty() && active_tasks_ == 0) {
                idle_cv_.notify_all();
            }
        }
    }
}
