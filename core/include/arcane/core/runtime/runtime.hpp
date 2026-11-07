#pragma once

#include "..\util\thread_pool.hpp"
#include "worker_thread.hpp"

class Runtime {
public:
	Runtime() = default;

	~Runtime() { stop(); }

	void stop()
	{
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

private:
	ThreadPool _pool;
	WorkerThread _db{ "arcane-db" };
	WorkerThread _net{ "arcane-net" };
	WorkerThread _rules{ "arcane-rules" };
};