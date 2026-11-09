
#include "arcane/core/command/command_processor.hpp"
#include "arcane/core/command/command_registry.hpp"
#include "arcane/core/command/command.hpp"
#include "arcane/core/command/store.hpp"
#include "arcane/core/command/types.hpp"
#include "arcane/core/runtime/runtime.hpp"

#include "check.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <stdexcept>

using namespace std::chrono_literals;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

template<typename T>
T await(std::future<T>& future)
{
    ARCANE_CHECK(future.wait_for(5s) == std::future_status::ready);
    return future.get();
}

void await(std::future<void>& future)
{
    ARCANE_CHECK(future.wait_for(5s) == std::future_status::ready);
    future.get();
}

RejectCode reject_code(const CommandResult& result)
{
    ARCANE_CHECK(std::holds_alternative<Reject>(result));
    return std::get<Reject>(result).Code;
}

Delta get_delta(const CommandResult& result)
{
    ARCANE_CHECK(std::holds_alternative<Delta>(result));
    return std::get<Delta>(result);
}

std::int64_t read_counter(CommandProcessor& processor)
{
    std::int64_t value = -1;

    auto future = processor.inspect(
        [&value](const Store& store)
        {
            value = store.get("counter");
        });

    await(future);

    return value;
}

// ------------------------------------------------------------
// Test commands
// ------------------------------------------------------------

enum class FailureStage
{
    None,
    Authorize,
    Validate,
    Apply
};

class TestAddCommand : public Command
{
public:
    TestAddCommand(
        std::int64_t amount,
        std::int64_t limit = 1'000'000,
        bool permitted = true,
        FailureStage failure = FailureStage::None)
        : _amount(amount),
        _limit(limit),
        _permitted(permitted),
        _failure(failure)
    {
    }

    std::optional<Reject> authorize(
        [[maybe_unused]] const Store& store,
        ClientId client) const override
    {
        if (_failure == FailureStage::Authorize)
            throw std::runtime_error("authorize failure");

        if (!_permitted || client == 0)
            return Reject{ RejectCode::NotPermitted, "Forbidden" };

        return std::nullopt;
    }

    std::optional<Reject> validate(
        const Store& store) const override
    {
        if (_failure == FailureStage::Validate)
            throw std::runtime_error("validate failure");

        if (store.get("counter") + _amount > _limit)
            return Reject{ RejectCode::RuleViolation, "Limit exceeded" };

        return std::nullopt;
    }

    std::string apply(Store& store) override
    {
        if (_failure == FailureStage::Apply)
            throw std::runtime_error("apply failure");

        const auto previous = store.get("counter");
        store.set("counter", previous + _amount);

        return "counter updated";
    }

private:
    std::int64_t _amount;
    std::int64_t _limit;
    bool _permitted;
    FailureStage _failure;
};

// ------------------------------------------------------------
// Registry helpers
// ------------------------------------------------------------

void register_add(
    CommandRegistry& registry,
    std::int64_t limit = 1'000'000)
{
    registry.register_type(
        "counter.add",
        [limit](const std::string& payload)
        -> std::unique_ptr<Command>
        {
            std::int64_t amount{};

            const auto* begin = payload.data();
            const auto* end = begin + payload.size();

            auto [ptr, error] = std::from_chars(begin, end, amount);

            if (error != std::errc{} || ptr != end || amount <= 0)
                return nullptr;

            return std::make_unique<TestAddCommand>(
                amount, limit);
        });
}

void register_failing_command(
    CommandRegistry& registry,
    FailureStage stage)
{
    registry.register_type(
        "counter.fail",
        [stage]([[maybe_unused]] const std::string& payload)
        -> std::unique_ptr<Command>
        {
            return std::make_unique<TestAddCommand>(
                1, 1'000'000, true, stage);
        });
}

// ------------------------------------------------------------
// 1. Basic successful command
// ------------------------------------------------------------

void test_valid_command()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto future = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "5");

    auto result = await(future);

    ARCANE_CHECK(std::holds_alternative<Delta>(result));

    auto delta = get_delta(result);
    std::cout << delta.rev << std::endl;
    ARCANE_CHECK(delta.rev == 1);
    ARCANE_CHECK(read_counter(processor) == 5);
}

// ------------------------------------------------------------
// 2. Unknown command
// ------------------------------------------------------------

void test_unknown_command()
{
    Runtime runtime;

    CommandRegistry registry;
    CommandProcessor processor(registry, runtime);

    auto future = processor.submit(
        CommandMeta{ 1, 1 }, "unknown.command", "");

    auto result = await(future);

    ARCANE_CHECK(reject_code(result) == RejectCode::UnknownCommand);
    ARCANE_CHECK(read_counter(processor) == 0);
}

// ------------------------------------------------------------
// 3. Bad schema
// ------------------------------------------------------------

void test_bad_schema()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto future = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "abc");

    auto result = await(future);

    ARCANE_CHECK(reject_code(result) == RejectCode::BadSchema);
    ARCANE_CHECK(read_counter(processor) == 0);
}

// ------------------------------------------------------------
// 4. Unauthorized client
// ------------------------------------------------------------

void test_not_permitted()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto future = processor.submit(
        CommandMeta{ 0, 1 }, "counter.add", "5");

    auto result = await(future);

    ARCANE_CHECK(reject_code(result) == RejectCode::NotPermitted);
    ARCANE_CHECK(read_counter(processor) == 0);
}

// ------------------------------------------------------------
// 5. Rule violation
// ------------------------------------------------------------

void test_rule_violation()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry, 10);

    CommandProcessor processor(registry, runtime);

    auto future = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "11");

    auto result = await(future);

    ARCANE_CHECK(reject_code(result) == RejectCode::RuleViolation);
    ARCANE_CHECK(read_counter(processor) == 0);
}

// ------------------------------------------------------------
// 6. Rejections do not consume revisions
// ------------------------------------------------------------

void test_rejection_does_not_consume_revision()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto rejected = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "abc");

    auto accepted = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "5");

    auto rejected_result = await(rejected);
    auto accepted_result = await(accepted);

    ARCANE_CHECK(
        reject_code(rejected_result) == RejectCode::BadSchema);

    ARCANE_CHECK(
        get_delta(accepted_result).rev == 1);

    ARCANE_CHECK(read_counter(processor) == 5);
}

// ------------------------------------------------------------
// 7. Exceptions must not terminate processor
// ------------------------------------------------------------

void test_apply_exception_recovery()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);
    register_failing_command(registry, FailureStage::Apply);

    CommandProcessor processor(registry, runtime);

    auto failing = processor.submit(
        CommandMeta{ 1, 1 }, "counter.fail", "");

    auto succeeding = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "5");

    auto failed_result = await(failing);
    auto successful_result = await(succeeding);

    ARCANE_CHECK(
        reject_code(failed_result) == RejectCode::Internal);

    ARCANE_CHECK(
        get_delta(successful_result).rev == 1);

    ARCANE_CHECK(read_counter(processor) == 5);
}

// ------------------------------------------------------------
// 8. Exceptions from authorize and validate
// ------------------------------------------------------------

void test_authorize_exception()
{
    Runtime runtime;

    CommandRegistry registry;
    register_failing_command(registry, FailureStage::Authorize);
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto failed = processor.submit(
        CommandMeta{ 1, 1 }, "counter.fail", "");

    auto succeeded = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "3");

    auto failed_result = await(failed);
    auto succeeded_result = await(succeeded);

    ARCANE_CHECK(
        reject_code(failed_result) == RejectCode::Internal);

    ARCANE_CHECK(get_delta(succeeded_result).rev == 1);
}

void test_validate_exception()
{
    Runtime runtime;

    CommandRegistry registry;
    register_failing_command(registry, FailureStage::Validate);
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto failed = processor.submit(
        CommandMeta{ 1, 1 }, "counter.fail", "");

    auto succeeded = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "3");

    auto failed_result = await(failed);
    auto succeeded_result = await(succeeded);

    ARCANE_CHECK(
        reject_code(failed_result) == RejectCode::Internal);

    ARCANE_CHECK(get_delta(succeeded_result).rev == 1);
}

// ------------------------------------------------------------
// 9. Listener receives accepted commands only
// ------------------------------------------------------------

void test_listener_notifications()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    std::mutex mutex;
    std::condition_variable cv;

    std::vector<Revision> revisions;
    std::vector<CommandMeta> metadata;

    processor.add_listener(
        [&](const Delta& delta, const CommandMeta& meta)
        {
            {
                std::lock_guard lock(mutex);

                revisions.push_back(delta.rev);
                metadata.push_back(meta);
            }

            cv.notify_one();
        });

    auto first = processor.submit(
        CommandMeta{ 1, 10 }, "counter.add", "1");

    auto rejected = processor.submit(
        CommandMeta{ 1, 11 }, "counter.add", "abc");

    auto second = processor.submit(
        CommandMeta{ 2, 20 }, "counter.add", "2");

    await(first);
    await(rejected);
    await(second);

    {
        std::unique_lock lock(mutex);

        ARCANE_CHECK(cv.wait_for(
            lock, 5s,
            [&] { return revisions.size() == 2; }));
    }

    ARCANE_CHECK(revisions[0] == 1);
    ARCANE_CHECK(revisions[1] == 2);

    ARCANE_CHECK(metadata[0].client == 1);
    ARCANE_CHECK(metadata[0].seq == 10);

    ARCANE_CHECK(metadata[1].client == 2);
    ARCANE_CHECK(metadata[1].seq == 20);
}

// ------------------------------------------------------------
// 10. Throwing listener must not terminate processor
// ------------------------------------------------------------

void test_throwing_listener()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    processor.add_listener(
        [](const Delta&, const CommandMeta&)
        {
            throw std::runtime_error("listener failure");
        });

    auto first = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "1");

    auto second = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "1");

    ARCANE_CHECK(get_delta(await(first)).rev == 1);
    ARCANE_CHECK(get_delta(await(second)).rev == 2);

    ARCANE_CHECK(read_counter(processor) == 2);
}

// ------------------------------------------------------------
// 11. Listener can submit another command
// ------------------------------------------------------------

void test_listener_can_submit()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    std::promise<std::future<CommandResult>> nested_promise;
    auto nested_future = nested_promise.get_future();

    std::atomic<bool> called{ false };

    processor.add_listener(
        [&](const Delta& delta, const CommandMeta&)
        {
            if (delta.rev == 1 && !called.exchange(true))
            {
                auto future = processor.submit(
                    CommandMeta{ 2, 1 }, "counter.add", "5");

                nested_promise.set_value(std::move(future));
            }
        });

    auto first = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "1");

    ARCANE_CHECK(get_delta(await(first)).rev == 1);

    auto nested = await(nested_future);

    ARCANE_CHECK(get_delta(await(nested)).rev == 2);
    ARCANE_CHECK(read_counter(processor) == 6);
}

// ------------------------------------------------------------
// 12. Concurrent submissions
// ------------------------------------------------------------

void test_concurrent_submissions()
{
    constexpr int THREAD_COUNT = 8;
    constexpr int COMMANDS_PER_THREAD = 1000;
    constexpr int TOTAL_COMMANDS =
        THREAD_COUNT * COMMANDS_PER_THREAD;

    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    std::mutex futures_mutex;
    std::vector<std::future<CommandResult>> futures;
    futures.reserve(TOTAL_COMMANDS);

    std::vector<std::thread> threads;

    for (int t = 0; t < THREAD_COUNT; ++t)
    {
        threads.emplace_back([&, t]
            {
                std::vector<std::future<CommandResult>> local;
                local.reserve(COMMANDS_PER_THREAD);

                for (int i = 0; i < COMMANDS_PER_THREAD; ++i)
                {
                    local.push_back(processor.submit(
                        CommandMeta{
                            static_cast<ClientId>(t + 1),
                            static_cast<Seq>(i + 1)
                        },
                        "counter.add",
                        "1"));
                }

                std::lock_guard lock(futures_mutex);

                for (auto& future : local)
                    futures.push_back(std::move(future));
            });
    }

    for (auto& thread : threads)
        thread.join();

    ARCANE_CHECK(futures.size() == TOTAL_COMMANDS);

    std::vector<Revision> revisions;
    revisions.reserve(TOTAL_COMMANDS);

    for (auto& future : futures)
    {
        auto result = await(future);

        ARCANE_CHECK(std::holds_alternative<Delta>(result));

        revisions.push_back(get_delta(result).rev);
    }

    std::sort(revisions.begin(), revisions.end());

    ARCANE_CHECK(revisions.size() == TOTAL_COMMANDS);

    for (int i = 0; i < TOTAL_COMMANDS; ++i)
    {
        ARCANE_CHECK(
            revisions[i] == static_cast<Revision>(i + 1));
    }

    ARCANE_CHECK(read_counter(processor) == TOTAL_COMMANDS);
}

// ------------------------------------------------------------
// 13. Inspection executes on processor thread
// ------------------------------------------------------------

void test_inspect_ordering()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto first = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "10");

    auto inspection = processor.inspect(
        [](const Store& store)
        {
            ARCANE_CHECK(store.get("counter") == 10);
        });

    auto second = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "5");

    await(first);
    await(inspection);
    await(second);

    ARCANE_CHECK(read_counter(processor) == 15);
}

// ------------------------------------------------------------
// 15. Runtime integration
// ------------------------------------------------------------

void test_runtime_integration()
{
    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto future = processor.submit(
        CommandMeta{ 1, 1 }, "counter.add", "42");

    auto result = await(future);

    ARCANE_CHECK(std::holds_alternative<Delta>(result));
    ARCANE_CHECK(get_delta(result).rev == 1);
    ARCANE_CHECK(read_counter(processor) == 42);
}

// ------------------------------------------------------------
// 14. Shutdown drains pending commands
// ------------------------------------------------------------
void test_stop_with_pending_commands()
{
    constexpr int COMMAND_COUNT = 500;

    Runtime runtime;

    CommandRegistry registry;
    register_add(registry);

    CommandProcessor processor(registry, runtime);

    std::vector<std::future<CommandResult>> futures;

    for (int i = 0; i < COMMAND_COUNT; ++i)
    {
        futures.push_back(processor.submit(
            CommandMeta{ 1, static_cast<Seq>(i + 1) },
            "counter.add",
            "1"));
    }

    std::int64_t final_value = 0;

    auto inspection = processor.inspect(
        [&](const Store& store)
        {
            final_value = store.get("counter");
        });

    processor.stop();

    for (auto& future : futures)
    {
        auto result = await(future);
        ARCANE_CHECK(std::holds_alternative<Delta>(result));
    }

    await(inspection);

    ARCANE_CHECK(final_value == COMMAND_COUNT);

    processor.stop();
}

void test_factory_exception()
{
    Runtime runtime;

    CommandRegistry registry;

    registry.register_type(
        "counter.throw",
        [](const std::string&)
        -> std::unique_ptr<Command>
        {
            throw std::runtime_error("factory failure");
        });

    register_add(registry);

    CommandProcessor processor(registry, runtime);

    auto failed = processor.submit(
        CommandMeta{ 1, 1 }, "counter.throw", "");

    auto succeeded = processor.submit(
        CommandMeta{ 1, 2 }, "counter.add", "5");

    ARCANE_CHECK(
        reject_code(await(failed)) == RejectCode::Internal);

    ARCANE_CHECK(get_delta(await(succeeded)).rev == 1);
    ARCANE_CHECK(read_counter(processor) == 5);
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    test_valid_command();
    test_unknown_command();
    test_bad_schema();
    test_not_permitted();
    test_rule_violation();
    test_rejection_does_not_consume_revision();
    

    test_apply_exception_recovery();
    test_authorize_exception();
    test_validate_exception();

    test_listener_notifications();
    test_throwing_listener();
    test_listener_can_submit();

    test_concurrent_submissions();
    test_inspect_ordering();
    test_stop_with_pending_commands();

    test_runtime_integration();

    return 0;
}
