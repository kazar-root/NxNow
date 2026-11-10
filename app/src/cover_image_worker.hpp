#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <array>
#include <vector>
#include <system_error>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nxnow
{
class CoverImageWorker
{
  public:
    static constexpr std::size_t kPendingLimit = 10;

    explicit CoverImageWorker(std::size_t concurrency = 1)
    {
        concurrency = std::clamp(concurrency, std::size_t {1}, active_.size());
        threads_.reserve(concurrency);
        for (std::size_t i = 0; i < concurrency; ++i)
        {
            try { threads_.emplace_back([this, i] { Run(i); }); }
            catch (const std::system_error&)
            {
                if (threads_.empty()) throw;
                break; // Keep the first worker if the second cannot start.
            }
        }
    }
    ~CoverImageWorker() { Stop(); }

    bool TrySubmit(std::shared_ptr<std::atomic_bool> cancelled, std::function<void()> work)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
            return false;
        std::erase_if(pending_, [](const Job& job) { return job.cancelled->load(); });
        if (pending_.size() == kPendingLimit)
            return false;
        pending_.push_back({std::move(cancelled), std::move(work)});
        wake_.notify_one();
        return true;
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
            for (const auto& active : active_)
                if (active) active->store(true);
            for (auto& job : pending_)
                job.cancelled->store(true);
            pending_.clear();
            wake_.notify_all();
        }
        for (auto& thread : threads_)
            if (thread.joinable()) thread.join();
    }

  private:
    struct Job
    {
        std::shared_ptr<std::atomic_bool> cancelled;
        std::function<void()> work;
    };

    void Run(std::size_t slot)
    {
#ifdef __SWITCH__
        svcSetThreadPriority(CUR_THREAD_HANDLE, 0x3B);
#endif
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;)
        {
            wake_.wait(lock, [this] { return stopped_ || !pending_.empty(); });
            if (stopped_)
                return;
            Job job = std::move(pending_.front());
            pending_.pop_front();
            active_[slot] = job.cancelled;
            lock.unlock();
            if (!job.cancelled->load())
            {
                try
                {
                    job.work();
                }
                catch (...)
                {
                }
            }
            lock.lock();
            active_[slot].reset();
        }
    }

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> pending_;
    std::array<std::shared_ptr<std::atomic_bool>, 2> active_;
    bool stopped_ = false;
    std::vector<std::thread> threads_;
};
}
