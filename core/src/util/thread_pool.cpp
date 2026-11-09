#include "arcane\core\util\thread_pool.hpp"

ThreadPool::ThreadPool(std::size_t n) : ThreadPool(n, make_channels<Job>())
{
}

ThreadPool::ThreadPool(std::size_t n, Channels _channels) : send(std::move(_channels.first)), recv(std::move(_channels.second))
{
	if (n == 0) n = std::max(1u, std::thread::hardware_concurrency());
	for (std::size_t i = 0; i < n; ++i)
	{
		threads.emplace_back([this] {
			while (auto job = recv.pop()) (*job)();
		});
	}
}

ThreadPool::~ThreadPool()
{
	stop();
}

void ThreadPool::stop()
{
	send.close();
	for (auto& t : threads)
	{
		if (t.joinable())
		{
			t.join();
		}
	}
}

std::size_t ThreadPool::size() const
{
	return threads.size();
}