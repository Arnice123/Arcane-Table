// Benchmarks for the visibility backends (ARCH-04 spike, Piece 8).
//
// Build and run from the RELEASE preset only. Debug numbers are meaningless:
//   cmake --preset release -DARCANE_BUILD_BENCHMARKS=ON
//   cmake --build build/release
//   build/release/tests/bench_vision --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
// Report the MEDIAN rows in the ADR, together with the machine details Google
// Benchmark prints at the top (add CPU model, compiler + version and thread count).
//
// Every benchmark is a template over the backend type, so adding the grid and
// shadow-map backends later is one registration line each at the bottom.
//
// All benchmarks use the canonical stress scene (seed 1). Scene generation and
// set_occluders() happen outside the timed loops, except in the benchmark that
// times set_occluders() itself.

#include <benchmark/benchmark.h>

#include "arcane/core/vision/polygon_backend.hpp"
#include "stress_scene.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace {

    // The canonical scene, built once and shared by every benchmark.
    const StressScene& scene() {
        static const StressScene s = make_stress_scene(1);
        return s;
    }

    // All 112 sources: 100 lights then 12 viewers.
    const std::vector<Viewer>& allSources() {
        static const std::vector<Viewer> sources = [] {
            std::vector<Viewer> v = scene().lights;
            v.insert(v.end(), scene().viewers.begin(), scene().viewers.end());
            return v;
            }();
        return sources;
    }

    // A backend with the scene's walls already loaded.
    template <typename Backend>
    std::unique_ptr<Backend> loadedBackend() {
        auto b = std::make_unique<Backend>();
        b->set_occluders(scene().walls);
        return b;
    }

    // Reports a "per_source" column: wall-clock time divided by sources handled.
    void reportPerSource(benchmark::State& state, std::size_t sourcesPerIteration) {
        state.counters["per_source"] = benchmark::Counter(
            static_cast<double>(sourcesPerIteration),
            benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
    }

    // -------------------------------------------------------------------------
    // "A wall changed": set_occluders with the 500 walls
    // (for the polygon backend this includes splitting crossing walls and building the tree)
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_SetOccluders(benchmark::State& state) {
        Backend b;
        for (auto _ : state) {
            b.set_occluders(scene().walls);
            benchmark::ClobberMemory();
        }
        state.counters["walls"] = static_cast<double>(scene().walls.size());
    }

    // -------------------------------------------------------------------------
    // Unit cost: one compute() for one source.
    // Each iteration takes the next of the 112 sources, so the time is an average
    // over all of them rather than one lucky (or unlucky) viewer.
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_ComputeOne(benchmark::State& state) {
        const auto b = loadedBackend<Backend>();
        const auto& sources = allSources();
        std::size_t i = 0;
        for (auto _ : state) {
            auto field = b->compute(sources[i]);
            benchmark::DoNotOptimize(field);
            i = (i + 1) % sources.size();
        }
    }

    // -------------------------------------------------------------------------
    // Loading a scene / worst-case refresh: all 112 sources
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_FullRecompute(benchmark::State& state) {
        const auto b = loadedBackend<Backend>();
        const auto& sources = allSources();
        for (auto _ : state) {
            for (const Viewer& v : sources) {
                auto field = b->compute(v);
                benchmark::DoNotOptimize(field);
            }
        }
        reportPerSource(state, sources.size());
    }

    // -------------------------------------------------------------------------
    // Common case during play: one token moves, so one source is recomputed.
    // The viewer steps along a small circle so every iteration is a real move
    // (a fresh position each time, not the same answer repeated).
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_SingleSourceUpdate(benchmark::State& state) {
        const auto b = loadedBackend<Backend>();
        Viewer v = scene().viewers[0];
        const Vec2 home = v.origin;
        int step = 0;
        for (auto _ : state) {
            // 64 positions on a circle of radius 0.5 around the viewer's home.
            const float a = static_cast<float>(step) * (6.2831853f / 64.0f);
            v.origin = Vec2{ home.x + 0.5f * std::cos(a), home.y + 0.5f * std::sin(a) };
            step = (step + 1) % 64;

            auto field = b->compute(v);
            benchmark::DoNotOptimize(field);
        }
    }

    // -------------------------------------------------------------------------
    // "Who sees what?": 12 viewers x 1,000 tokens = 12,000 contains() queries.
    // The 12 fields are computed before timing; only the queries are timed.
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_Contains12k(benchmark::State& state) {
        const auto b = loadedBackend<Backend>();
        std::vector<std::unique_ptr<VisibilityField>> fields;
        for (const Viewer& v : scene().viewers) fields.push_back(b->compute(v));
        const auto& tokens = scene().tokens;

        std::size_t seen = 0;
        for (auto _ : state) {
            for (const auto& f : fields)
                for (const Vec2& t : tokens)
                    seen += f->contains(t) ? 1 : 0;
            benchmark::DoNotOptimize(seen);
        }
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(fields.size() * tokens.size()));
        // How many tokens were visible per pass, as a sanity check that the work is real.
        state.counters["visible"] = static_cast<double>(seen) / static_cast<double>(state.iterations());
    }

    // -------------------------------------------------------------------------
    // The rules' other question: 12,000 line_of_sight() checks (viewer -> token),
    // without building a field. Not in the spec's table, but it is what canSee /
    // lineOfSight from scripts will call, so it is worth a row in the ADR.
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_LineOfSight12k(benchmark::State& state) {
        const auto b = loadedBackend<Backend>();
        const auto& viewers = scene().viewers;
        const auto& tokens = scene().tokens;

        std::size_t clear = 0;
        for (auto _ : state) {
            for (const Viewer& v : viewers)
                for (const Vec2& t : tokens)
                    clear += b->line_of_sight(v.origin, t) ? 1 : 0;
            benchmark::DoNotOptimize(clear);
        }
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(viewers.size() * tokens.size()));
    }

    // -------------------------------------------------------------------------
    // Memory: rough size of the fields, i.e. the cost of caching one per source.
    // Only outline() is visible through the interface, so this estimates each
    // field from its point count (the polygon field stores one point and one
    // angle per vertex). Backend-internal structures (the tree, wall copies)
    // are not visible here; estimate those separately in the ADR.
    // -------------------------------------------------------------------------
    template <typename Backend>
    void BM_FieldMemory(benchmark::State& state) {
        const auto b = loadedBackend<Backend>();
        const auto& sources = allSources();
        double totalPoints = 0;
        for (auto _ : state) {
            totalPoints = 0;
            for (const Viewer& v : sources) {
                const auto outline = b->compute(v)->outline();
                totalPoints += static_cast<double>(outline.size());
            }
        }
        const double perField = totalPoints / static_cast<double>(sources.size());
        state.counters["points_per_field"] = perField;
        state.counters["bytes_per_field"] = perField * static_cast<double>(sizeof(Vec2) + sizeof(float));
        state.counters["bytes_all_112"] = totalPoints * static_cast<double>(sizeof(Vec2) + sizeof(float));
    }

}  // namespace

// -----------------------------------------------------------------------------
// Registrations. Add GridBackend / ShadowMapBackend lines here when they exist.
// -----------------------------------------------------------------------------

BENCHMARK_TEMPLATE(BM_SetOccluders, PolygonBackend)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_ComputeOne, PolygonBackend)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_FullRecompute, PolygonBackend)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleSourceUpdate, PolygonBackend)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Contains12k, PolygonBackend)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_LineOfSight12k, PolygonBackend)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_FieldMemory, PolygonBackend)->Unit(benchmark::kMillisecond)->Iterations(1);

BENCHMARK_MAIN();