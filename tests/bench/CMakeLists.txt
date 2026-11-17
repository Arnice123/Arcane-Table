# Google Benchmark targets. Enable with -DARCANE_BUILD_BENCHMARKS=ON; the
# "benchmark" package must be installed. Story TESTCI-04 adds the canonical
# stress scene and the CI regression gate (fail on a >10% regression).

if(ARCANE_BUILD_BENCHMARKS)
  find_package(benchmark REQUIRED)

  add_executable(bench_placeholder placeholder_bench.cpp)
  target_link_libraries(bench_placeholder PRIVATE arcane::core benchmark::benchmark benchmark::benchmark_main)

  # vision_bench.cpp defines its own main (BENCHMARK_MAIN), so no benchmark_main here.
  add_executable(bench_vision vision_bench.cpp)
  target_link_libraries(bench_vision PRIVATE arcane::core benchmark::benchmark)
  target_include_directories(bench_vision PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../support")

  set_target_properties(bench_placeholder bench_vision PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()