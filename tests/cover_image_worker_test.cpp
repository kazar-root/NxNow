#include "cover_image_worker.hpp"

#include <cassert>
#include <future>
#include <stdexcept>
#include <vector>

int main()
{
    constexpr auto limit = nxnow::CoverImageWorker::kPendingLimit;
    static_assert(limit == 10);
    nxnow::CoverImageWorker worker;
    std::promise<void> started;
    std::promise<void> release;
    const auto release_signal = release.get_future().share();
    auto active = std::make_shared<std::atomic_bool>(false);
    assert(worker.TrySubmit(active, [&] {
        started.set_value();
        release_signal.wait();
    }));
    started.get_future().wait();

    std::vector<int> executed;
    std::vector<std::shared_ptr<std::atomic_bool>> tokens;
    std::promise<void> finished;
    for (int index = 0; index < 60; ++index)
    {
        auto token = std::make_shared<std::atomic_bool>(false);
        tokens.push_back(token);
        const bool accepted = worker.TrySubmit(token, [&, index] {
            executed.push_back(index);
        });
        assert(accepted == (static_cast<std::size_t>(index) < limit));
    }
    for (const auto& token : tokens)
        assert(!token->load());
    tokens[0]->store(true);
    assert(worker.TrySubmit(std::make_shared<std::atomic_bool>(false), [&] {
        executed.push_back(60);
        finished.set_value();
    }));
    release.set_value();
    finished.get_future().wait();
    assert(executed.size() == limit);
    assert(executed.front() == 1 && executed.back() == 60);
    std::promise<void> retried;
    for (int index = static_cast<int>(limit); index < 60; ++index)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!worker.TrySubmit(tokens[index], [&, index] {
            executed.push_back(index);
            if (index == 59)
                retried.set_value();
        })) {
            assert(std::chrono::steady_clock::now() < deadline);
            std::this_thread::yield();
        }
    }
    retried.get_future().wait();
    assert(executed.size() == 60);
    auto failed = std::make_shared<std::atomic_bool>(false);
    assert(worker.TrySubmit(failed, [] { throw std::runtime_error("transfer failed"); }));
    std::promise<void> after_failure;
    assert(worker.TrySubmit(std::make_shared<std::atomic_bool>(false), [&] {
        after_failure.set_value();
    }));
    after_failure.get_future().wait();
    assert(!failed->load());
    worker.Stop();
    auto after_stop = std::make_shared<std::atomic_bool>(false);
    assert(!worker.TrySubmit(after_stop, [] { assert(false); }));
    assert(!after_stop->load());
    worker.Stop();

    nxnow::CoverImageWorker stopping;
    std::promise<void> second_started;
    std::promise<void> second_release;
    auto second_signal = second_release.get_future().share();
    auto second_active = std::make_shared<std::atomic_bool>(false);
    assert(stopping.TrySubmit(second_active, [&] {
        second_started.set_value();
        second_signal.wait();
        assert(second_active->load());
    }));
    second_started.get_future().wait();
    auto pending = std::make_shared<std::atomic_bool>(false);
    assert(stopping.TrySubmit(pending, [] { assert(false); }));
    auto stopped = std::async(std::launch::async, [&] { stopping.Stop(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!second_active->load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(second_active->load());
    second_release.set_value();
    stopped.get();
    assert(pending->load());

    // Both workers must make progress while one download is blocked.
    nxnow::CoverImageWorker parallel(2);
    std::promise<void> blocked, other_started, unblock;
    const auto unblock_signal = unblock.get_future().share();
    assert(parallel.TrySubmit(std::make_shared<std::atomic_bool>(false), [&] {
        blocked.set_value(); unblock_signal.wait();
    }));
    blocked.get_future().wait();
    assert(parallel.TrySubmit(std::make_shared<std::atomic_bool>(false), [&] { other_started.set_value(); }));
    assert(other_started.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    unblock.set_value();
    parallel.Stop();
}
