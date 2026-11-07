#pragma once

#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <utility>
#include <cstddef>
#include <iostream>

template <class T>
struct ChannelState {
	std::queue<T> queue;
	std::mutex lock;
	std::condition_variable cv;
	bool is_closed = false;
};

template <class T>
class Channel;

template <typename T>
std::pair<Channel<T>, Channel<T>> make_channels();

template <class T>
class Channel {
private:
	std::shared_ptr<ChannelState<T>> state;
	bool is_sender;

	explicit Channel(std::shared_ptr<ChannelState<T>> _state, bool _is_sender) : state(std::move(_state)), is_sender(_is_sender) {}

	template <typename U>
	friend std::pair<Channel<U>, Channel<U>> make_channels();

public:
	Channel(Channel const&) = delete;
	Channel(Channel &&) = default;
	Channel& operator=(Channel&) = delete;
	Channel& operator=(Channel&&) = default;

	~Channel()
	{
		close();
	}

	// Returns false if the channel is closed (item not enqueued).
	bool push(T item)
	{
		do {
			if (!is_sender || !state) break;
			std::lock_guard<std::mutex> lock{ state->lock };
			if (state->is_closed) break;
			state->queue.push(std::move(item));
			state->cv.notify_one();
			return true;
		} while (false);
		return false;
	}

	// Blocks until an item is available. Returns nullopt only when the
	// channel is closed AND drained.
	std::optional<T> pop()
	{
		do {
			if (is_sender || !state) break;
			std::unique_lock lock{ state->lock };
			state->cv.wait(lock, [this] 
			{ 
				return !state->queue.empty() || state->is_closed; 
			});
			if (state->queue.empty()) break;
			auto x = std::move(state->queue.front());
			state->queue.pop();
			return x;			
		} while (false);
		return std::nullopt;
	}

	// never blocks
	std::optional<T> try_pop()
	{
		if (is_sender || !state) return std::nullopt;
		if (state->queue.empty() || state->is_closed) return std::nullopt;
		auto x = std::move(state->queue.front());
		state->queue.pop();
		return x;
	}

	// No more pushes accepted; wakes all waiting poppers; existing items
	// are still delivered.
	bool close()
	{
		do {
			if (!state) break;
			std::lock_guard lock{ state->lock };
			if (state->is_closed) break;
			state->is_closed = true;
			state->cv.notify_one();
			return true;
		} while (false);
		return false;
	}

	bool closed() const
	{
		std::lock_guard lock{ state->lock };
		return state->is_closed;
	}

	std::size_t size() const
	{
		std::lock_guard lock{ state->lock };
		return state->queue.size();
	}
};

template<typename T>
std::pair<Channel<T>, Channel<T>> make_channels()
{
	auto ptr = std::make_shared<ChannelState<T>>();
	Channel<T> sender{ ptr, true };
	Channel<T> receiver{ std::move(ptr), false };
	return { std::move(sender), std::move(receiver) };
}