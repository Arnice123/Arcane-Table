#include <cstring>

#include "arcane/core/module.hpp"
#include "arcane/net/module.hpp"
#include "check.hpp"

int main() {
  ARCANE_CHECK(std::strcmp(arcane::net::module_name(), "net") == 0);
  // net depends on core, so core headers and symbols must be reachable.
  ARCANE_CHECK(std::strcmp(arcane::core::module_name(), "core") == 0);
  return 0;
}
