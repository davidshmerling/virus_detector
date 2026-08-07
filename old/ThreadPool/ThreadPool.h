#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool {
public:
    ThreadPool(
        std::size_t worker_count,
        std::size_t queue_capacity);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool enqueue(std::function<void()> task);

    void wait();

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;

    std::mutex mutex_;

    // Wakes a worker when a new task arrives.
    std::condition_variable task_cv_;

    // Wakes the enumerator when queue space is available.
    std::condition_variable space_cv_;

    // Wakes wait() when all work is finished.
    std::condition_variable idle_cv_;

    std::size_t queue_capacity_ = 0;
    std::size_t active_tasks_ = 0;

    bool stop_ = false;
};
