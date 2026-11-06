# tests: everything runs through ctest.
#   unit/         per-module tests; core and net tests link no Qt at all
#   integration/  host + clients (network simulator arrives with NET-06)
#   ui/           Qt Test on the offscreen platform (only if the client builds)
#   fuzz/         libFuzzer targets (ARCANE_ENABLE_FUZZING, Clang only)
#   bench/        Google Benchmark (ARCANE_BUILD_BENCHMARKS)
#   fixtures/     frozen databases, sample campaigns, malformed assets
#   golden/       renderer golden images
#
# The tests below use a tiny CHECK macro (support/check.hpp). Story TESTCI-01
# replaces it with Catch2 or GoogleTest.

add_library(arcane_test_support INTERFACE)
target_include_directories(arcane_test_support INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/support")

# arcane_add_test(<ctest name> SOURCES ... LIBS ... [ENVIRONMENT VAR=value])
# The executable is named test_<name with dots replaced by underscores>.
function(arcane_add_test name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBS;ENVIRONMENT")

  string(REPLACE "." "_" exe "test_${name}")
  add_executable(${exe} ${ARG_SOURCES})
  target_link_libraries(${exe} PRIVATE
    ${ARG_LIBS} arcane_test_support arcane_warnings arcane_sanitizers)
  set_target_properties(${exe} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

  add_test(NAME ${name} COMMAND ${exe})
  if(ARG_ENVIRONMENT)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${ARG_ENVIRONMENT}")
  endif()
endfunction()

# Source-level guard for the no-Qt/GL rule (the configure-time check covers
# the link graph; this covers #includes and CMake references).
add_test(NAME arch.no_qt_gl_in_headless_modules
  COMMAND "${CMAKE_COMMAND}"
          -DARCANE_ROOT=${PROJECT_SOURCE_DIR}
          -P "${PROJECT_SOURCE_DIR}/cmake/CheckNoQtGl.cmake")

add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(ui)
add_subdirectory(fuzz)
add_subdirectory(bench)
