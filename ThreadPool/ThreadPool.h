#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Generic bounded worker pool. Knows nothing about files or scanning.
// Starts a fixed number of workers; the queue blocks when full.
class ThreadPool {
public:
    // Starts the worker threads with the default size and queue capacity.
    ThreadPool();

    // Stops accepting work, joins all workers, and discards leftover tasks.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueues `task`. Blocks while the queue is full. Returns false if the
    // pool is stopping.
    bool enqueue(std::function<void()> task);

    // Blocks until the queue is empty and no worker is running a task.
    void wait();

private:
    // Worker thread body: waits for tasks and executes them until stop.
    void workerLoop();

    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;

    std::mutex mutex_;
    std::condition_variable task_available_cv_;
    std::condition_variable queue_space_cv_;
    std::condition_variable all_tasks_done_cv_;

    std::size_t queue_capacity_ = 0;
    std::size_t active_tasks_ = 0;
    bool stop_ = false;
};
