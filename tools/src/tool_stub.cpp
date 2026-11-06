#include <cstdio>

#include "arcane/core/version.hpp"

#ifndef ARCANE_TOOL_NAME
#error "ARCANE_TOOL_NAME must be defined by tools/CMakeLists.txt"
#endif

int main() {
  std::printf("arcane-%s %s (stub, not implemented yet)\n", ARCANE_TOOL_NAME,
              arcane::version_string);
  return 0;
}
