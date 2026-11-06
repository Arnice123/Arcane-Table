#include <cstdio>
#include <cstring>

#include "arcane/core/version.hpp"
#include "arcane/net/module.hpp"

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--version") == 0) {
    std::printf("arcane-server %s\n", arcane::version_string);
    return 0;
  }

  std::printf("arcane-server %s (headless, %s module linked)\n",
              arcane::version_string, arcane::net::module_name());
  std::printf("Nothing to serve yet: see backlog stories NET-01 to NET-06.\n");
  return 0;
}
