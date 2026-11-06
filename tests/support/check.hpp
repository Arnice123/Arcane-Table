#pragma once

// Minimal assertion helper so the scaffold needs no test framework.
// Replaced by Catch2 or GoogleTest in story TESTCI-01.

#include <cstdio>
#include <cstdlib>

#define ARCANE_CHECK(condition)                                              \
  do {                                                                       \
    if (!(condition)) {                                                      \
      std::fprintf(stderr, "%s:%d: CHECK FAILED: %s\n", __FILE__, __LINE__, \
                   #condition);                                              \
      std::exit(1);                                                          \
    }                                                                        \
  } while (false)
