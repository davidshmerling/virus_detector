#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Generic bounded worker pool. Knows nothing about files or scanning.
class ThreadPool {
public:
    explicit ThreadPool(
        std::size_t worker_count = 16,
        std::size_t queue_capacity = 1024);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Blocks while the queue is full. Returns false if the pool is stopping.
    bool enqueue(std::function<void()> task);

    // Blocks until the queue is empty and no worker is running a task.
    void wait();

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;

    std::mutex mutex_;
    std::condition_variable task_cv_;
    std::condition_variable space_cv_;
    std::condition_variable idle_cv_;

    std::size_t queue_capacity_ = 0;
    std::size_t active_tasks_ = 0;
    bool stop_ = false;
};
