#include <cstring>

#include "arcane/core/module.hpp"
#include "arcane/net/module.hpp"
#include "check.hpp"

// Placeholder: proves the integration test target builds and links both
// headless modules. Replace with a real host/client scenario.
int main() {
  ARCANE_CHECK(std::strcmp(arcane::core::module_name(), "core") == 0);
  ARCANE_CHECK(std::strcmp(arcane::net::module_name(), "net") == 0);
  return 0;
}
