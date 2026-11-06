#include <cstring>

#include "arcane/core/module.hpp"
#include "arcane/core/version.hpp"
#include "check.hpp"

int main() {
  ARCANE_CHECK(std::strcmp(arcane::core::module_name(), "core") == 0);
  ARCANE_CHECK(arcane::version_string[0] != '\0');
  ARCANE_CHECK(arcane::version_major >= 0);
  return 0;
}
