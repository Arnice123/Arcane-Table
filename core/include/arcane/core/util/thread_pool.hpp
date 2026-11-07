#pragma once

#include <cstddef>
#include <future>
#include <thread>
#include <vector>
#include <tuple>
#include "channel.hpp"

class ThreadPool {
public:
	explicit ThreadPool(std::size_t n = 0);
	~ThreadPool();

	template <class F>
	auto submit(F&& f) -> std::future<std::invoke_result_t<F>>
	{
		using R = std::invoke_result_t<F>;
		auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
		auto fut = task->get_future();
		send.push([task] { (*task)(); });
		return fut;
	}

	void stop();
	std::size_t size() const;
private:
	using Job = std::function<void()>;
	using Channels = std::pair<Channel<Job>, Channel<Job>>;
	Channel<Job> send, recv;
	std::vector<std::jthread> threads;

	ThreadPool(std::size_t n, Channels _channels);
};