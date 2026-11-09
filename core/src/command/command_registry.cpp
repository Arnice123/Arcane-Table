#include "arcane\core\command\command_registry.hpp"

bool CommandRegistry::register_type(const std::string& name, Factory factory)
{
	auto [it, success] = list.try_emplace(name, factory);
	if (!success)  return false; 
	return true;
}

CommandRegistry::CreateResult CommandRegistry::create(const std::string& name, const std::string& payload) const
{
	auto comm = list.find(name);
	if (comm == list.end())
	{
		return Reject{ RejectCode::UnknownCommand, "Command is unknown, register the type first" };
	}
	std::unique_ptr<Command> res = comm->second(payload);

	if (res == nullptr)
	{
		return Reject{ RejectCode::BadSchema, "Malformed Payload" };
	}
	return res;
}