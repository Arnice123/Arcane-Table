#pragma once

#include <optional>
#include "arcane/core/command/types.hpp"
#include "arcane/core/command/store.hpp"

class Command {
public:
	virtual ~Command() = default;

	// Do the game rules allow this, given the current state?
	// Out: nullopt if OK, else a Reject (RuleViolation / Conflict)
	virtual std::optional<Reject> validate(const Store& store) const = 0;

	// Is the client allowed to do this?
	// Out: nullopt if OK, else Reject(NotPermitted)
	virtual std::optional<Reject> authorize(const Store& store, ClientId client) const = 0;

	// make the change, only should be called after validated and authorized
	// Out: a short human-readable description of what changed.
	virtual std::string apply(Store& store) = 0;
};