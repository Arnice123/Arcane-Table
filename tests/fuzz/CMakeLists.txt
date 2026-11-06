# libFuzzer targets. Enable with -DARCANE_ENABLE_FUZZING=ON using Clang
# (preset: dev-fuzz). Real targets (dice parser, protocol, bundle importer)
# arrive with stories DICE-05, NET-02 and BUNDLES-01; when they do, compile the
# library under test with -fsanitize=fuzzer-no-link so it is instrumented too.
#
# Run a target for a while:   build/dev-fuzz/tests/fuzz_placeholder -max_total_time=60

if(ARCANE_ENABLE_FUZZING)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_placeholder placeholder_fuzz.cpp)
    target_link_libraries(fuzz_placeholder PRIVATE arcane::core)
    target_compile_options(fuzz_placeholder PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_placeholder PRIVATE -fsanitize=fuzzer,address)
    set_target_properties(fuzz_placeholder PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME fuzz.placeholder COMMAND fuzz_placeholder -runs=1000)
  else()
    message(WARNING "ARCANE_ENABLE_FUZZING needs Clang (found ${CMAKE_CXX_COMPILER_ID}); fuzz targets skipped")
  endif()
endif()
