#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class JobSystem {
public:
    explicit JobSystem(size_t threadCount = std::thread::hardware_concurrency());
    ~JobSystem();

    void Submit(std::function<void()> job);
    void Wait(); // blocks until all submitted jobs finish

private:
    void WorkerLoop();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_jobs;

    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::condition_variable m_finishedCondition;

    std::atomic<bool> m_stop{false};
    std::atomic<int> m_activeJobs{0};
    size_t m_pendingJobs = 0;
};