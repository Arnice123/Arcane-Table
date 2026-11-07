#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <future>
#include <iostream>
#include <thread>
#include <stdexcept>

#include "../util/channel.hpp"
#include "../util/named_thread.hpp"

class WorkerThread {
private:
	using Job = std::function<void()>;
	using Channels = std::pair<Channel<Job>, Channel<Job>>;
	Channel<Job> send, recv;
	std::atomic<bool> _alive{ false };
	std::string name;
	std::jthread thread;

	WorkerThread(std::string _name, Channels channel);

public:
	explicit WorkerThread(std::string _name);
	~WorkerThread();

	void post(Job job);
	void stop();
	bool alive() const;

};