#pragma once

#include "arcane\core\util\thread_pool.hpp"
#include "arcane\core\runtime\worker_thread.hpp"

class Runtime {
public:
	Runtime() = default;

	~Runtime() { stop(); }

	void stop()
	{
		_cmdProcessor.stop();
		_net.stop();
		_rules.stop();
		_pool.stop();
		_db.stop();
	}

	// for heavy parallel work
	ThreadPool& pool() { return _pool; }
	WorkerThread& db() { return _db; }
	WorkerThread& net() { return _net; }
	WorkerThread& rules() { return _rules; }
	WorkerThread& cmdProcessor() { return _cmdProcessor; }

private:
	ThreadPool _pool;
	WorkerThread _db{ "arcane-db" };
	WorkerThread _net{ "arcane-net" };
	WorkerThread _rules{ "arcane-rules" };
	WorkerThread _cmdProcessor{ "arcane-cmd" };
};