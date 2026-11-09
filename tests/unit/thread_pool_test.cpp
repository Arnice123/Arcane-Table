#include <iostream>
#include <vector>
#include <future>
#include <atomic>
#include <numeric>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <string>

#include "arcane\core\util\thread_pool.hpp"
#include "check.hpp"

void test_basic_submission()
{
    std::cout << "[TEST] Basic submission\n";

    ThreadPool pool(4);

    auto future = pool.submit([] {
        return 42;
        });

    ARCANE_CHECK(future.get() == 42);
}


void test_multiple_jobs()
{
    std::cout << "[TEST] 1,000+ jobs\n";

    constexpr int JOB_COUNT = 100000;

    ThreadPool pool(4);
    std::atomic<int> completed{ 0 };

    std::vector<std::future<int>> futures;
    futures.reserve(JOB_COUNT);

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        futures.push_back(pool.submit([i, &completed] {
            completed++;
            return i * 2;
        }));
    }

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        ARCANE_CHECK(futures[i].get() == i * 2);
    }

    ARCANE_CHECK(completed == JOB_COUNT);

    std::cout << "  Completed: " << completed << '\n';
}

void test_different_return_types()
{
    std::cout << "[TEST] Different return types\n";

    ThreadPool pool(4);

    auto int_future = pool.submit([] {
        return 123;
    });

    auto string_future = pool.submit([] {
        return std::string("ThreadPool");
    });

    auto void_future = pool.submit([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    ARCANE_CHECK(int_future.get() == 123);
    ARCANE_CHECK(string_future.get() == "ThreadPool");

    void_future.get();
}

void test_concurrent_execution()
{
    std::cout << "[TEST] Concurrent execution\n";

    constexpr int JOB_COUNT = 8;

    ThreadPool pool(4);

    std::atomic<int> active{ 0 };
    std::atomic<int> max_active{ 0 };

    std::vector<std::future<void>> futures;

    for (int i = 0; i < JOB_COUNT; ++i)
    {
        futures.push_back(pool.submit([&] {
            int current = ++active;

            int previous_max = max_active.load();

            while (current > previous_max &&
                !max_active.compare_exchange_weak(
                    previous_max, current))
            {
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100));

            --active;
        }));
    }

    for (auto& future : futures)
        future.get();

    ARCANE_CHECK(max_active > 1);

    std::cout << "  Maximum concurrent jobs: "
        << max_active << '\n';
}

void test_exception_propagation()
{
    std::cout << "[TEST] Exception propagation\n";

    ThreadPool pool(2);

    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
    });

    bool caught = false;

    try
    {
        future.get();
    }
    catch (const std::runtime_error& e)
    {
        caught = true;
        ARCANE_CHECK(std::string(e.what()) == "Test exception");
    }

    ARCANE_CHECK(caught);
}

void test_default_constructor()
{
    std::cout << "[TEST] Default constructor\n";

    ThreadPool pool;

    auto future = pool.submit([] {
        return 99;
    });

    ARCANE_CHECK(future.get() == 99);

    std::cout << "  Pool size: " << pool.size() << '\n';
}

void test_stop()
{
    std::cout << "[TEST] Explicit stop\n";

    ThreadPool pool(4);

    std::atomic<int> completed{ 0 };

    std::vector<std::future<void>> futures;

    for (int i = 0; i < 100; ++i)
    {
        futures.push_back(pool.submit([&] {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(5));

            ++completed;
        }));
    }

    pool.stop();

    for (auto& future : futures)
        future.get();

    ARCANE_CHECK(completed == 100);
}

void test_destruction_with_pending_jobs()
{
    std::cout << "[TEST] Destruction with pending jobs\n";

    std::atomic<int> completed{ 0 };

    {
        ThreadPool pool(4);

        for (int i = 0; i < 1000; ++i)
        {
            pool.submit([&completed] {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(100));

                ++completed;
            });
        }
    }

    ARCANE_CHECK(completed == 1000);

    std::cout << "  Completed before destruction: "
        << completed << '\n';
}

int main()
{
    try
    {
        test_basic_submission();      
        test_multiple_jobs();
        test_different_return_types();
        test_concurrent_execution();
        test_exception_propagation();
        test_default_constructor();
        test_stop();
        test_destruction_with_pending_jobs();
        

        std::cout << "\nALL TESTS PASSED\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nTEST FAILED: " << e.what() << '\n';
        return 1;
    }

    return 0;
}