
#include "arcane/core/command/demo_commands.hpp"

#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>

namespace
{
    constexpr std::int64_t COUNTER_LIMIT = 1'000'000;

    class CounterAddCommand final : public Command
    {
    public:
        explicit CounterAddCommand(std::int64_t amount)
            : _amount(amount)
        {
        }

        // Everyone except client 0 can add to the counter.
        std::optional<Reject> authorize(
            [[maybe_unused]] const Store& store,
            ClientId client
        ) const override
        {
            if (client == 0)
            {
                return Reject{
                    RejectCode::NotPermitted,
                    "Client 0 cannot modify the counter"
                };
            }

            return std::nullopt;
        }

        // Reject additions that would exceed the counter limit.
        std::optional<Reject> validate(const Store& store) const override
        {
            const std::int64_t current = store.get("counter");

            if (_amount > COUNTER_LIMIT - current)
            {
                return Reject{
                    RejectCode::RuleViolation,
                    "Counter cannot exceed 1,000,000"
                };
            }

            return std::nullopt;
        }

        // Apply the accepted command.
        std::string apply(Store& store) override
        {
            const std::int64_t previous = store.get("counter");
            const std::int64_t updated = previous + _amount;

            store.set("counter", updated);

            return "counter " +
                std::to_string(previous) +
                " -> " +
                std::to_string(updated);
        }

    private:
        std::int64_t _amount;
    };

    // Parses a whole-number payload.
    // Returns nullptr if payload is malformed.
    std::unique_ptr<Command> create_counter_add(
        const std::string& payload
    )
    {
        std::int64_t amount = 0;

        const char* begin = payload.data();
        const char* end = begin + payload.size();

        const auto [ptr, error] = std::from_chars(begin, end, amount);

        // Reject empty strings, malformed numbers, overflow,
        // and trailing characters.
        if (error != std::errc{} || ptr != end)
        {
            return nullptr;
        }

        // Only positive additions allowed.
        if (amount <= 0)
        {
            return nullptr;
        }

        return std::make_unique<CounterAddCommand>(amount);
    }
}

void register_demo_commands(CommandRegistry& registry)
{
    registry.register_type("counter.add", create_counter_add);
}