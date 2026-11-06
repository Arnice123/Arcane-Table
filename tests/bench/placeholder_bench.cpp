#include <benchmark/benchmark.h>

#include "arcane/core/module.hpp"

static void BM_ModuleName(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(arcane::core::module_name());
  }
}
BENCHMARK(BM_ModuleName);
