#include "check.hpp"
#include "arcane\core\runtime\worker_thread.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ---------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------

void test_starts_alive()
{
    WorkerThread worker("test-worker");

    ARCANE_CHECK(worker.alive());

    worker.stop();

    ARCANE_CHECK(!worker.alive());
}

void test_stop_twice()
{
    WorkerThread worker("test-worker");

    worker.stop();
    ARCANE_CHECK(!worker.alive());

    worker.stop();
    ARCANE_CHECK(!worker.alive());
}

void test_destructor_stops_thread()
{
    std::atomic<bool> finished = false;

    {
        WorkerThread worker("destructor-test");

        worker.post([&] {
            finished = true;
            });
    }

    ARCANE_CHECK(finished.load());
}

// ---------------------------------------------------------
// Job execution
// ---------------------------------------------------------

void test_executes_posted_job()
{
    WorkerThread worker("execution-test");

    std::promise<void> completed;
    auto future = completed.get_future();

    worker.post([&] {
        completed.set_value();
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    worker.stop();
}

void test_job_runs_on_worker_thread()
{
    WorkerThread worker("thread-id-test");

    std::promise<std::thread::id> result;
    auto future = result.get_future();

    const auto caller_id = std::this_thread::get_id();

    worker.post([&] {
        result.set_value(std::this_thread::get_id());
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    const auto worker_id = future.get();

    ARCANE_CHECK(worker_id != caller_id);

    worker.stop();
}

void test_jobs_execute_in_fifo_order()
{
    WorkerThread worker("fifo-test");

    std::vector<int> results;
    std::mutex results_mutex;

    std::promise<void> completed;
    auto future = completed.get_future();

    constexpr int JOB_COUNT = 100;

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        worker.post([&, i] {
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(i);
            }

            if (i == JOB_COUNT - 1)
                completed.set_value();
            });
    }

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    ARCANE_CHECK(results.size() == JOB_COUNT);

    for (int i = 0; i < JOB_COUNT; ++i)
        ARCANE_CHECK(results[i] == i);

    worker.stop();
}

// ---------------------------------------------------------
// Multiple jobs / concurrency
// ---------------------------------------------------------

void test_executes_all_posted_jobs()
{
    WorkerThread worker("multiple-jobs-test");

    constexpr int JOB_COUNT = 1000;

    std::atomic<int> executed = 0;

    std::promise<void> completed;
    auto future = completed.get_future();

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        worker.post([&] {
            if (++executed == JOB_COUNT)
                completed.set_value();
            });
    }

    ARCANE_CHECK(future.wait_for(5s) == std::future_status::ready);
    ARCANE_CHECK(executed == JOB_COUNT);

    worker.stop();
}

void test_accepts_jobs_from_multiple_threads()
{
    WorkerThread worker("multi-producer-test");

    constexpr int PRODUCER_COUNT = 8;
    constexpr int JOBS_PER_PRODUCER = 100;

    constexpr int TOTAL_JOBS =
        PRODUCER_COUNT * JOBS_PER_PRODUCER;

    std::atomic<int> executed = 0;

    std::promise<void> completed;
    auto future = completed.get_future();

    std::vector<std::thread> producers;

    for (int i = 0; i < PRODUCER_COUNT; ++i)
    {
        producers.emplace_back([&] {
            for (int j = 0; j < JOBS_PER_PRODUCER; ++j)
            {
                worker.post([&] {
                    if (++executed == TOTAL_JOBS)
                        completed.set_value();
                    });
            }
            });
    }

    for (auto& producer : producers)
        producer.join();

    ARCANE_CHECK(future.wait_for(5s) == std::future_status::ready);
    ARCANE_CHECK(executed == TOTAL_JOBS);

    worker.stop();
}

// ---------------------------------------------------------
// Shutdown behavior
// ---------------------------------------------------------

void test_stop_drains_queued_jobs()
{
    WorkerThread worker("drain-test");

    constexpr int JOB_COUNT = 100;

    std::atomic<int> executed = 0;

    std::promise<void> started;
    auto started_future = started.get_future();

    // Block worker until all other jobs have been queued.
    std::promise<void> release;
    auto release_future = release.get_future();

    worker.post([&] {
        started.set_value();
        release_future.wait();
        });

    ARCANE_CHECK(
        started_future.wait_for(2s) == std::future_status::ready
    );

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        worker.post([&] {
            ++executed;
            });
    }

    // Release first job, then shut down.
    release.set_value();

    worker.stop();

    ARCANE_CHECK(executed == JOB_COUNT);
    ARCANE_CHECK(!worker.alive());
}

void test_drops_jobs_after_stop()
{
    WorkerThread worker("drop-test");

    worker.stop();

    std::atomic<bool> executed = false;

    worker.post([&] {
        executed = true;
        });

    ARCANE_CHECK(!executed.load());
    ARCANE_CHECK(!worker.alive());
}

void test_stop_waits_for_running_job()
{
    WorkerThread worker("wait-test");

    std::promise<void> started;
    auto started_future = started.get_future();

    std::promise<void> release;
    auto release_future = release.get_future();

    std::atomic<bool> finished = false;

    worker.post([&] {
        started.set_value();
        release_future.wait();
        finished = true;
        });

    ARCANE_CHECK(
        started_future.wait_for(2s) == std::future_status::ready
    );

    std::thread releaser([&] {
        std::this_thread::sleep_for(50ms);
        release.set_value();
        });

    worker.stop();

    releaser.join();

    ARCANE_CHECK(finished.load());
    ARCANE_CHECK(!worker.alive());
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------

int main()
{
    test_starts_alive();
    test_stop_twice();
    test_destructor_stops_thread();

    test_executes_posted_job();
    test_job_runs_on_worker_thread();
    test_jobs_execute_in_fifo_order();

    test_executes_all_posted_jobs();
    test_accepts_jobs_from_multiple_threads();

    test_stop_drains_queued_jobs();
    test_drops_jobs_after_stop();
    test_stop_waits_for_running_job();

    std::printf("All WorkerThread tests passed!\n");

    return 0;
}