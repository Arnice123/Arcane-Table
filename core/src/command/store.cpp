#include "arcane\core\command\store.hpp"

std::int64_t Store::get(const std::string& key) const
{
	auto it = mp.find(key);
	return it != mp.end() ? it->second : 0;
}

void Store::set(const std::string& key, std::int64_t value)
{
	mp.insert_or_assign(std::move(key), value);
}