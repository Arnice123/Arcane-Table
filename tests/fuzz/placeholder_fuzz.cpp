#include <cstddef>
#include <cstdint>

#include "arcane/core/module.hpp"

// Placeholder fuzz target: shows the libFuzzer entry point and keeps the
// fuzz build wired up. Replace with a real parser target (see DICE-05).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > 0 && data[0] == 0xFF && arcane::core::module_name() == nullptr) {
    __builtin_trap();
  }
  return 0;
}
