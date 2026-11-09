#include "arcane\core\command\command_processor.hpp"

CommandProcessor::CommandProcessor(CommandRegistry reg, Runtime& rt) : _reg(std::move(reg)), _rev_count(1), _is_closed(false)
{
	_wThread = &(rt.cmdProcessor());
}

CommandProcessor::~CommandProcessor()
{
	stop();
}

// Standing in for networking layer
// Registers a callback to run on accepted commands
void CommandProcessor::add_listener(Listener listener)
{
	_listeners.push_back(listener);
}

/// <summary>
/// Outputs a future that holds whether command was accepted/rejected
/// </summary>
/// <param name="meta">Who is asking</param>
/// <param name="name">The name of the command</param>
/// <param name="payload">The payload to run on</param>
/// <returns></returns>
std::future<CommandResult> CommandProcessor::submit(CommandMeta meta, std::string name, std::string payload)
{
	std::shared_ptr<std::promise<CommandResult>> prom = std::make_shared<std::promise<CommandResult>>();
	auto res = prom->get_future();

	if (_is_closed)
	{
		prom->set_value(Reject(RejectCode::ClosedProcessor, "This processor has closed"));
		return res;
	}

	_wThread->post([this, meta = std::move(meta), prom, name = std::move(name), payload = std::move(payload)]() mutable {
		try {
			CommandRegistry::CreateResult cr = _reg.create(name, payload);
			if (cr.index() == 1) {
				prom->set_value(std::get<Reject>(cr));
				return;
			}
			std::unique_ptr<Command> cmd = std::move(std::get<std::unique_ptr<Command>>(cr));
			std::optional<Reject> authRes = cmd->authorize(_store, meta.client);
			if (authRes)
			{
				prom->set_value(authRes.value());
				return;
			}

			std::optional<Reject> validateRes = cmd->validate(_store);
			if (validateRes)
			{
				prom->set_value(validateRes.value());
				return;
			}

			std::string appRes = cmd->apply(_store);

			Delta d(_rev_count, appRes);
			prom->set_value(CommandResult(d));
			++_rev_count;

			for (unsigned int i = 0; i < _listeners.size(); ++i)
			{
				_listeners[i](d, meta);
			}
		}
		catch (const std::exception& e) {
			prom->set_value(Reject(RejectCode::Internal, "An error occured internally while trying submit command"));
			std::cerr << "[ERROR]: " << e.what() << std::endl;
		}
	});	
	return res;
}

// Lets other threads read the Store safely
std::future<void> CommandProcessor::inspect(std::function<void(const Store&)> reader)
{
	std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void>>();
	auto fut = prom->get_future();	

	if (_is_closed)
	{
		prom->set_exception(std::make_exception_ptr(std::runtime_error("CommandProcessor stopped")));
		return fut;
	}

	_wThread->post([reader = std::move(reader), prom, this]() mutable {
		try {
			reader(_store);
			prom->set_value();
		}
		catch (const std::exception& e)
		{
			prom->set_exception(std::make_exception_ptr(std::runtime_error("CommandProcessor stopped")));
		}
	});

	return fut;
}

// No new submissions; commands already queued finshed; thread is joined
void CommandProcessor::stop()
{
	_wThread->post([this]() {
		_is_closed = true;
	});	
}