#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "arcane\core\command\command.hpp"

class CommandRegistry {
public:
	using Factory = std::function<std::unique_ptr<Command>(const std::string& payload)>;
	using CreateResult = std::variant<std::unique_ptr<Command>, Reject>;

	// Registers a command type under name
	// Takes in a unique name and factor that builds the command from the payload string, factory returns a nullptr if it is malformed
	// Outputs true if it makes the command, false if name is taken
	bool register_type(const std::string& name, Factory factory);

	// Takes in a name and payload
	// outputs a ready command, or Reject(UnknownCommand)/Reject(BadSchema)
	CreateResult create(const std::string& name, const std::string& payload) const;
private:
	std::unordered_map<std::string, Factory> list;
};