#include "JobSystem.h"
#include "../Log.h"

JobSystem::JobSystem(size_t threadCount) {
    Log::Info("JobSystem starting with {} worker threads", threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back(&JobSystem::WorkerLoop, this);
    }
}

JobSystem::~JobSystem() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_condition.notify_all();
    for (auto& worker : m_workers) {
        worker.join();
    }
}

void JobSystem::Submit(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_jobs.push(std::move(job));
        m_pendingJobs++;
    }
    m_condition.notify_one();
}

void JobSystem::Wait() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_finishedCondition.wait(lock, [this] {
        return m_pendingJobs == 0;
    });
}

void JobSystem::WorkerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] {
                return m_stop || !m_jobs.empty();
            });

            if (m_stop && m_jobs.empty())
                return;

            job = std::move(m_jobs.front());
            m_jobs.pop();
        }

        job();

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_pendingJobs--;
            if (m_pendingJobs == 0)
                m_finishedCondition.notify_all();
        }
    }
}