#pragma once

#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "arcane\core\command\command_registry.hpp"
#include "arcane\core\command\command.hpp"
#include "arcane\core\command\types.hpp"
#include "arcane\core\command\store.hpp"
#include "arcane\core\runtime\runtime.hpp"

class CommandProcessor {
public:
	using Listener = std::function<void(const Delta&, const CommandMeta&)>;

	explicit CommandProcessor(CommandRegistry reg, Runtime& rt);

	~CommandProcessor();

	// Standing in for networking layer
	// Registers a callback to run on accepted commands
	void add_listener(Listener listener);

	/// <summary>
	/// Outputs a future that holds whether command was accepted/rejected
	/// </summary>
	/// <param name="meta">Who is asking</param>
	/// <param name="name">The name of the command</param>
	/// <param name="payload">The payload to run on</param>
	/// <returns></returns>
	std::future<CommandResult> submit(CommandMeta meta, std::string name, std::string payload);

	// Lets other threads read the Store safely
	std::future<void> inspect(std::function<void(const Store&)> reader);

	// No new submissions; commands already queued finshed; thread is joined
	void stop();
private:
	std::vector<Listener> _listeners;
	CommandRegistry _reg;
	WorkerThread* _wThread;
	Store _store;
	Revision _rev_count;
	bool _is_closed;
};