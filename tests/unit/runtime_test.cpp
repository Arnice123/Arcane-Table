#include "check.hpp"
#include "arcane\core\runtime\runtime.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include <string>
#include <mutex>

using namespace std::chrono_literals;

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------

void test_runtime_initializes_all_workers()
{
    Runtime runtime;

    ARCANE_CHECK(runtime.pool().size() > 0);
    ARCANE_CHECK(runtime.db().alive());
    ARCANE_CHECK(runtime.net().alive());
    ARCANE_CHECK(runtime.rules().alive());

    runtime.stop();
}

void test_runtime_accessors_return_same_instances()
{
    Runtime runtime;

    ARCANE_CHECK(&runtime.pool() == &runtime.pool());
    ARCANE_CHECK(&runtime.db() == &runtime.db());
    ARCANE_CHECK(&runtime.net() == &runtime.net());
    ARCANE_CHECK(&runtime.rules() == &runtime.rules());

    runtime.stop();
}

// ---------------------------------------------------------
// Worker execution
// ---------------------------------------------------------

void test_db_worker_executes_jobs()
{
    Runtime runtime;

    std::promise<void> completed;
    auto future = completed.get_future();

    runtime.db().post([&] {
        completed.set_value();
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    runtime.stop();
}

void test_net_worker_executes_jobs()
{
    Runtime runtime;

    std::promise<void> completed;
    auto future = completed.get_future();

    runtime.net().post([&] {
        completed.set_value();
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    runtime.stop();
}

void test_rules_worker_executes_jobs()
{
    Runtime runtime;

    std::promise<void> completed;
    auto future = completed.get_future();

    runtime.rules().post([&] {
        completed.set_value();
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    runtime.stop();
}

void test_pool_executes_jobs()
{
    Runtime runtime;

    auto future = runtime.pool().submit([] {
        return 42;
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);
    ARCANE_CHECK(future.get() == 42);

    runtime.stop();
}

// ---------------------------------------------------------
// Thread separation
// ---------------------------------------------------------

void test_workers_run_on_separate_threads()
{
    Runtime runtime;

    std::promise<std::thread::id> db_promise;
    std::promise<std::thread::id> net_promise;
    std::promise<std::thread::id> rules_promise;

    auto db_future = db_promise.get_future();
    auto net_future = net_promise.get_future();
    auto rules_future = rules_promise.get_future();

    runtime.db().post([&] {
        db_promise.set_value(std::this_thread::get_id());
        });

    runtime.net().post([&] {
        net_promise.set_value(std::this_thread::get_id());
        });

    runtime.rules().post([&] {
        rules_promise.set_value(std::this_thread::get_id());
        });

    ARCANE_CHECK(db_future.wait_for(2s) == std::future_status::ready);
    ARCANE_CHECK(net_future.wait_for(2s) == std::future_status::ready);
    ARCANE_CHECK(rules_future.wait_for(2s) == std::future_status::ready);

    auto db_id = db_future.get();
    auto net_id = net_future.get();
    auto rules_id = rules_future.get();

    ARCANE_CHECK(db_id != net_id);
    ARCANE_CHECK(db_id != rules_id);
    ARCANE_CHECK(net_id != rules_id);

    runtime.stop();
}

void test_pool_does_not_execute_on_caller_thread()
{
    Runtime runtime;

    auto caller_id = std::this_thread::get_id();

    auto future = runtime.pool().submit([] {
        return std::this_thread::get_id();
        });

    ARCANE_CHECK(future.wait_for(2s) == std::future_status::ready);

    ARCANE_CHECK(future.get() != caller_id);

    runtime.stop();
}

// ---------------------------------------------------------
// Independent execution
// ---------------------------------------------------------

void test_all_subsystems_execute_jobs()
{
    Runtime runtime;

    constexpr int JOB_COUNT = 100;

    std::atomic<int> executed = 0;

    std::promise<void> completed;
    auto future = completed.get_future();

    auto job = [&] {
        if (++executed == JOB_COUNT)
            completed.set_value();
        };

    for (int i = 0; i < JOB_COUNT / 4; ++i)
    {
        runtime.db().post(job);
        runtime.net().post(job);
        runtime.rules().post(job);

        runtime.pool().submit(job);
    }

    ARCANE_CHECK(future.wait_for(5s) == std::future_status::ready);
    ARCANE_CHECK(executed == JOB_COUNT);

    runtime.stop();
}

void test_blocked_worker_does_not_block_other_workers()
{
    Runtime runtime;

    std::promise<void> db_started;
    auto db_started_future = db_started.get_future();

    std::promise<void> release_db;
    auto release_db_future = release_db.get_future();

    std::promise<void> net_completed;
    auto net_future = net_completed.get_future();

    runtime.db().post([&] {
        db_started.set_value();
        release_db_future.wait();
        });

    ARCANE_CHECK(
        db_started_future.wait_for(2s) == std::future_status::ready
    );

    runtime.net().post([&] {
        net_completed.set_value();
        });

    ARCANE_CHECK(net_future.wait_for(2s) == std::future_status::ready);

    release_db.set_value();

    runtime.stop();
}

// ---------------------------------------------------------
// Shutdown
// ---------------------------------------------------------

void test_stop_stops_all_workers()
{
    Runtime runtime;

    runtime.stop();

    ARCANE_CHECK(!runtime.db().alive());
    ARCANE_CHECK(!runtime.net().alive());
    ARCANE_CHECK(!runtime.rules().alive());
}

void test_stop_twice_is_safe()
{
    Runtime runtime;

    runtime.stop();
    runtime.stop();

    ARCANE_CHECK(!runtime.db().alive());
    ARCANE_CHECK(!runtime.net().alive());
    ARCANE_CHECK(!runtime.rules().alive());
}

void test_destructor_stops_all_workers()
{
    std::atomic<int> completed = 0;

    {
        Runtime runtime;

        runtime.db().post([&] {
            ++completed;
            });

        runtime.net().post([&] {
            ++completed;
            });

        runtime.rules().post([&] {
            ++completed;
            });
    }

    ARCANE_CHECK(completed == 3);
}

void test_stop_drains_worker_queues()
{
    Runtime runtime;

    constexpr int JOB_COUNT = 100;

    std::atomic<int> executed = 0;

    std::promise<void> started;
    auto started_future = started.get_future();

    std::promise<void> release;
    auto release_future = release.get_future();

    // Block DB worker until all jobs have been queued.
    runtime.db().post([&] {
        started.set_value();
        release_future.wait();
        });

    ARCANE_CHECK(
        started_future.wait_for(2s) == std::future_status::ready
    );

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        runtime.db().post([&] {
            ++executed;
            });
    }

    release.set_value();

    runtime.stop();

    ARCANE_CHECK(executed == JOB_COUNT);
}

void test_workers_reject_jobs_after_stop()
{
    Runtime runtime;

    runtime.stop();

    std::atomic<int> executed = 0;

    runtime.db().post([&] { ++executed; });
    runtime.net().post([&] { ++executed; });
    runtime.rules().post([&] { ++executed; });

    ARCANE_CHECK(executed == 0);
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------

int main()
{
    // Initialization
    test_runtime_initializes_all_workers();
    test_runtime_accessors_return_same_instances();

    // Execution
    test_db_worker_executes_jobs();
    test_net_worker_executes_jobs();
    test_rules_worker_executes_jobs();
    test_pool_executes_jobs();

    // Thread separation
    test_workers_run_on_separate_threads();
    test_pool_does_not_execute_on_caller_thread();

    // Independent execution
    test_all_subsystems_execute_jobs();
    test_blocked_worker_does_not_block_other_workers();

    // Shutdown
    test_stop_stops_all_workers();
    test_stop_twice_is_safe();
    test_destructor_stops_all_workers();
    test_stop_drains_worker_queues();
    test_workers_reject_jobs_after_stop();

    std::printf("All Runtime tests passed!\n");

    return 0;
}