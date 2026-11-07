#include "..\..\include\arcane\core\runtime\worker_thread.hpp"

WorkerThread::WorkerThread(std::string _name) : WorkerThread(_name, make_channels<Job>()) {}

WorkerThread::WorkerThread(std::string _name, Channels channel) : send(std::move(channel.first)), recv(std::move(channel.second)), name(std::move(_name))
{
	std::promise<void> start_promise;
	std::future<void> start_future = start_promise.get_future();
	thread = std::jthread([&start_promise, this]{
		set_thread_name(name);
		_alive = true;
		start_promise.set_value();
		while (auto job = recv.pop())
		{
			try { (*job)(); }
			catch (const std::exception& e) { std::cerr << "[ERROR] " << e.what() << std::endl; }
		}
		_alive = false;
	}); 
	start_future.wait();	
}

WorkerThread::~WorkerThread() 
{ 
	stop(); 
}

void WorkerThread::post(Job job)
{
	send.push(std::move(job));
}

void WorkerThread::stop()
{
	send.close();
	if (thread.joinable())	{ thread.join(); }
}

bool WorkerThread::alive() const
{
	return _alive;
}