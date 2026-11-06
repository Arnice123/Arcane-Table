# Google Benchmark targets. Enable with -DARCANE_BUILD_BENCHMARKS=ON; the
# "benchmark" package must be installed. Story TESTCI-04 adds the canonical
# stress scene and the CI regression gate (fail on a >10% regression).

if(ARCANE_BUILD_BENCHMARKS)
  find_package(benchmark REQUIRED)
  add_executable(bench_placeholder placeholder_bench.cpp)
  target_link_libraries(bench_placeholder PRIVATE
    arcane::core benchmark::benchmark benchmark::benchmark_main)
  set_target_properties(bench_placeholder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()
