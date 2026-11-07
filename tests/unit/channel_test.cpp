#include "..\..\core\include\arcane\core\util\channel.hpp"
#include "check.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

// --------------------------------------------------
// Test 1: Basic push and pop
// --------------------------------------------------

void test_basic_push_pop()
{
    auto [sender, receiver] = make_channels<int>();

    ARCANE_CHECK(sender.push(42));

    auto result = receiver.pop();

    std::cout << receiver.size() << std::endl;

    ARCANE_CHECK(result.has_value());
    ARCANE_CHECK(result.value() == 42);
    ARCANE_CHECK(receiver.size() == 0);

    std::cout << "[PASS] Basic push/pop\n";
}

// --------------------------------------------------
// Test 2: FIFO ordering
// --------------------------------------------------

void test_fifo()
{
    auto [sender, receiver] = make_channels<int>();

    ARCANE_CHECK(sender.push(10));
    ARCANE_CHECK(sender.push(20));
    ARCANE_CHECK(sender.push(30));

    ARCANE_CHECK(receiver.pop() == 10);
    ARCANE_CHECK(receiver.pop() == 20);
    ARCANE_CHECK(receiver.pop() == 30);

    std::cout << "[PASS] FIFO ordering\n";
}

// --------------------------------------------------
// Test 3: try_pop does not block
// --------------------------------------------------

void test_try_pop()
{
    auto [sender, receiver] = make_channels<int>();

    ARCANE_CHECK(!receiver.try_pop().has_value());

    ARCANE_CHECK(sender.push(100));

    auto result = receiver.try_pop();

    ARCANE_CHECK(result.has_value());
    ARCANE_CHECK(result.value() == 100);

    ARCANE_CHECK(!receiver.try_pop().has_value());

    std::cout << "[PASS] Non-blocking try_pop\n";
}

// --------------------------------------------------
// Test 4: Queue size
// --------------------------------------------------

void test_size()
{
    auto [sender, receiver] = make_channels<int>();

    ARCANE_CHECK(receiver.size() == 0);

    sender.push(1);
    sender.push(2);
    sender.push(3);

    ARCANE_CHECK(receiver.size() == 3);

    receiver.pop();

    ARCANE_CHECK(receiver.size() == 2);

    receiver.pop();
    receiver.pop();

    ARCANE_CHECK(receiver.size() == 0);

    std::cout << "[PASS] Queue size\n";
}

// --------------------------------------------------
// Test 5: Closing channel
// --------------------------------------------------

void test_close()
{
    auto [sender, receiver] = make_channels<int>();

    sender.push(10);
    sender.push(20);

    sender.close();

    ARCANE_CHECK(sender.closed());
    ARCANE_CHECK(receiver.closed());

    // Existing items should still be available.
    ARCANE_CHECK(receiver.pop() == 10);
    ARCANE_CHECK(receiver.pop() == 20);

    // Closed and drained channel should return nullopt.
    ARCANE_CHECK(!receiver.pop().has_value());

    // No more pushes accepted.
    ARCANE_CHECK(!sender.push(30));

    std::cout << "[PASS] Channel closing and draining\n";
}

// --------------------------------------------------
// Test 6: Blocking pop
// --------------------------------------------------

void test_blocking_pop()
{
    auto [sender, receiver] = make_channels<int>();

    auto result = std::async(std::launch::async, [&receiver]()
        {
            return receiver.pop();
        });

    ARCANE_CHECK(
        result.wait_for(std::chrono::milliseconds(100))
        == std::future_status::timeout
    );

    sender.push(42);

    ARCANE_CHECK(
        result.wait_for(std::chrono::seconds(2))
        == std::future_status::ready
    );

    ARCANE_CHECK(result.get() == 42);

    std::cout << "[PASS] Blocking pop and notification\n";
}

// --------------------------------------------------
// Test 7: Closing wakes blocked consumers
// --------------------------------------------------

void test_close_wakes_consumer()
{
    auto [sender, receiver] = make_channels<int>();

    auto result = std::async(std::launch::async, [&receiver]()
        {
            return receiver.pop();
        });

    ARCANE_CHECK(
        result.wait_for(std::chrono::milliseconds(100))
        == std::future_status::timeout
    );

    sender.close();

    ARCANE_CHECK(
        result.wait_for(std::chrono::seconds(2))
        == std::future_status::ready
    );

    ARCANE_CHECK(!result.get().has_value());

    std::cout << "[PASS] Close wakes blocked consumer\n";
}

// --------------------------------------------------
// Test 8: Concurrent producer/consumer
// --------------------------------------------------

void test_concurrent_access()
{
    constexpr int ITEM_COUNT = 10000;

    auto [sender, receiver] = make_channels<int>();

    std::thread producer([&sender]()
        {
            for (int i = 0; i < ITEM_COUNT; ++i)
            {
                ARCANE_CHECK(sender.push(i));
            }

            sender.close();
        });

    std::thread consumer([&receiver]()
        {
            for (int i = 0; i < ITEM_COUNT; ++i)
            {
                auto result = receiver.pop();

                ARCANE_CHECK(result.has_value());
                ARCANE_CHECK(result.value() == i);
            }

            ARCANE_CHECK(!receiver.pop().has_value());
        });

    producer.join();
    consumer.join();

    std::cout << "[PASS] Concurrent producer/consumer\n";
}

// --------------------------------------------------
// Main
// --------------------------------------------------

int main()
{
    test_basic_push_pop();
    test_fifo();
    test_try_pop();
    test_size();
    test_close();
    test_blocking_pop();
    test_close_wakes_consumer();
    test_concurrent_access();

    std::cout << "\nAll tests passed!\n";

    return 0;
}
