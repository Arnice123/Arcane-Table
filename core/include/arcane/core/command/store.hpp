#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

// a placeholder class to test the pipeline
class Store {
public:
	std::int64_t get(const std::string& key) const;
	void set(const std::string& key, std::int64_t value);
private:
	std::unordered_map<std::string, std::int64_t> mp;
};