# ADR 0001: Visibility backend for fog of war and lighting (Spike S1)

- **Status:** Proposed (provisional, see "Limitations")
- **Date:** 2026-11-16
- **Author:** Arya
- **Story:** ARCH-04 (Spike S1)
- **Design doc references:** §5.3.3 (visibility algorithm), §9.2 (performance budgets), decision log D-10, spike table S1

## 1. Context

Fog of war, token vision and dynamic lighting all depend on one question: from this point, given the walls, what can be seen? The design doc recommends exact 2D visibility polygons behind an `IVisibilityBackend` interface, but marks the choice `[DECIDE]` and lists two alternatives: GPU 1D shadow maps and grid shadowcasting (§5.3.3).

The choice has to be made before the grid, token and render epics are built. Three constraints from the design doc shape it:

1. **The host answers sight queries and runs headless.** `canSee`, `lineOfSight` and similar queries are host-authoritative (§5.3.3), and `core/` may not depend on Qt or OpenGL. Whatever backend serves these queries must run on the CPU with no GPU.
2. **Frame budget.** The canonical scene must hold 60 FPS (16.6 ms per frame), with a 30 FPS floor (§9.2).
3. **Scale.** Walls must scale to at least 10,000 segments through a spatial index (§5.3.2).

### Canonical stress scene (assumptions)

§9.2 fixes the counts (1,000 tokens, 500 walls, 100 lights, 12 viewers). It does not fix the map size or lengths, so this spike chose:

| Parameter | Value |
|---|---|
| Map size | 100 × 100 units (1 unit = 1 grid cell) |
| Walls | 500 segments, lengths 2–10 units, random positions |
| Lights | 100, radius 6–12 units |
| Viewers | 12, radius 12 units |
| Tokens | 1,000 at random positions (targets of visibility queries) |
| Random seed | 1 |

## 2. Options considered

| Option | Summary |
|---|---|
| **A. Visibility polygon** | Exact angular sweep over wall endpoints on the CPU. Produces one polygon per viewer. |
| **B. Grid shadowcasting** | Walks grid cells outward and tracks the range of visible slopes. Cheap and discrete. |
| **C. GPU 1D shadow map** | Stores the distance to the nearest wall for each of N angles, then compares per-pixel distances in a shader. |

Only option A was implemented and measured. See "Limitations".

## 3. Method

| Item | Value |
|---|---|
| Date and time of run | 2026-11-16 23:04 (UTC-7) |
| Binary | `build\release\tests\bench_vision.exe` (release preset, Google Benchmark) |
| CPU | 8 logical CPUs at 2496 MHz. The cache layout (L1 32 KiB ×4, L2 256 KiB ×4, L3 8 MiB ×1) suggests 4 physical cores with hyperthreading. |
| Repetitions | 10 per benchmark; the table reports the median |
| Command line | `--benchmark_repetitions=10 --benchmark_report_aggregates_only=true` |
| Threads used | 1 (single-threaded measurements) |

Set-up cost (`set_occluders`) and per-viewer cost (`compute`) were timed separately. All timings below are the median of the 10 repetitions.

## 4. Results (polygon backend)

| Measurement | Median | Notes |
|---|---|---|
| `set_occluders`, 500 walls | 533 µs | Cost of replacing the wall list ("a wall changed") |
| `compute`, one source | 46.1 µs | The unit cost |
| Full recompute, all 112 sources | 4.95 ms | About 44 µs per source; single thread |
| Single-source update after one move | 38.7 µs | The common case during play |
| `contains`, 12,000 queries (12 viewers × 1,000 tokens) | 115 µs | About 10 ns per query |
| `line_of_sight`, 12,000 queries | 6.24 ms | About 0.5 µs per query (CPU time 5.93 ms) |
| Field size | 2.45 KB each | About 204 vertices per field; 274 KB for all 112 fields |

Run-to-run variation (coefficient of variation) was 3.9–8.2% for the timing benchmarks, which is small compared with the margins below.

### Against the targets

| Target | Result | Margin |
|---|---|---|
| ≤ 0.4 ms per source (suggested pass bar) | 0.046 ms | about 8.7× under |
| Full recompute well inside the 16.6 ms frame budget | 4.95 ms single-threaded, about 30% of one frame | about 3.4× under, without using worker threads |
| Full recompute ≤ about 45 ms single-threaded (suggested bar) | 4.95 ms | about 9× under |

With four worker threads the full recompute should fall to roughly 1.2–1.5 ms. That figure is an **estimate** assuming near-linear scaling; it was not measured.

At 44 µs per source, about 377 sources would fit in one frame on a single thread if all were recomputed at once (16.6 ms ÷ 44 µs). In practice recomputation is incremental and time-sliced (§5.3.3), so the real ceiling is higher.

### Observations

- **`line_of_sight` is the slowest operation per query**, about 50 times slower than `contains` (roughly 0.5 µs against 10 ns). The figure is consistent with a linear scan of all 500 walls at about 1 ns per wall, which suggests this path does not use the spatial index. **Confirm this.** If true, it is the easiest optimization available, and it matters more as wall counts grow toward the 10,000 target.
- **The `contains` figure is optimistic for crowded scenes.** Only 131 of the 12,000 viewer-token pairs were visible, so most queries probably reject quickly because the token is far outside the view radius. A scene with many tokens close to the viewers would cost more per query.
- **The field-memory benchmark's time columns are not meaningful.** It ran with `iterations:1` and its CPU-time variation was over 200%. Only its counters (bytes and points per field) are used above.
- **Not yet measured:** accuracy against the brute-force oracle (test 7), and the speed-up from the spatial index compared with the naive polygon version. Were not collected

## 5. Analysis

**The polygon backend meets every target with a large margin on the canonical scene.** Per-source cost is under a tenth of the suggested bar, a full refresh of all 112 sources takes under a third of one frame on a single thread, and each field is small enough (2.45 KB) that caching a field per source is cheap.

**Why the alternatives were not measured, and what can still be said about them.** The statements below are design reasoning, not measurements.

- **Option C (GPU shadow map) cannot be the source of truth.** The host answers `canSee` and `lineOfSight` for rules and scripts and runs headless, with no GPU. A GPU technique could at most be a render-side effect that mirrors a CPU result. It also has a built-in angular resolution limit (one bucket covers about 2·π·d / N units at distance d).
- **Option B (grid shadowcasting) has a representation problem for tabletop maps.** Classic shadowcasting treats whole cells as walls. Walls in this engine are thin segments, often running along cell edges and at arbitrary angles (§5.3.2). Using it would require blocked-edge flags or a finer grid. Its accuracy cost on this scene was not measured.

**Cost of adopting the polygon backend:** it is the only one of the three where cost depends on how many walls lie near each viewer, so dense maps are its risk. Section 7 lists the triggers for revisiting.

## 6. Decision

Adopt the **visibility polygon backend (option A)** as the MVP implementation behind `IVisibilityBackend`.

Reasons, in order of weight:
1. It runs on the CPU, so it can serve the host's authoritative queries headless.
2. It is exact (subject to the accuracy numbers to be filled in above), which matters for rules such as cover and line of effect.
3. It meets the performance targets with at least a 3× margin on the canonical scene, single-threaded.
4. Its fields are small and immutable, so they can be cached and handed between threads safely (§2.3).

This confirms the design doc's recommendation for D-10 for the MVP. The decision log should record D-10 as "decided for MVP, provisional", linked to this ADR.

## 7. Consequences and when to revisit

**Easier:** the grid, token and render epics can be built against a stable interface. The renderer can turn each field's `outline()` into a mask.

**Harder or deferred:**
- If a dense or GPU-heavy lighting mode is wanted later (for example soft shadows with very many lights), a GPU shadow-map renderer could be added **for drawing only**, fed by the same CPU results.
- The alternatives are still unmeasured. Swapping backends is cheap because of the interface, but the comparison should be completed if any trigger below fires.

**Revisit this decision if any of these happen:**
- A realistic scene exceeds about 10,000 wall segments and per-source cost approaches the 0.4 ms bar.
- A full recompute exceeds about 8 ms single-threaded on target hardware.
- `line_of_sight` is on a hot path (for example per-token every frame) and cannot be sped up with the spatial index.
- Accuracy tests show disagreement with the oracle that matters for gameplay.

## 8. Interface

The interface used in this spike, with the changes made during implementation:

```
Viewer            { origin, radius }
VisibilityField   { contains(point) -> bool;  outline() -> vertices }   // immutable result
IVisibilityBackend{ name();  set_occluders(walls);
                    compute(viewer) -> VisibilityField;
                    line_of_sight(a, b) -> bool }
```

- Only the polygon backend has been implemented against this interface, so it has not yet been tested against a backend whose natural output is a cell set or a distance table. That check is part of the follow-up.

## 9. Limitations of this spike

- **Only one of the three candidates was implemented and measured.** The story's acceptance criteria call for prototyping or literature-benchmarking all three. This ADR therefore cannot claim that the polygon backend outperformed the others; it claims that it meets the requirements. Status is "Proposed" for this reason.
- The accuracy comparison (test 7) and the naive-versus-indexed polygon comparison are Not Collected.
- Measurements come from one machine and one compiler. They have not been repeated on Linux or on the CI runners.
- The stress-scene geometry (map size, wall lengths, radii) was chosen for this spike rather than taken from the design doc.

## 10. Follow-up

2. Either prototype options B and C, or complete a literature comparison with cited sources, and update this ADR. Track this as a small follow-up story if ARCH-04 is closed now.
3. Re-run the benchmark on Linux and on the CI runner and add the numbers here.
4. Update the design doc's decision log entry D-10.

## 11. How to reproduce

```
cmake --preset release -DARCANE_BUILD_BENCHMARKS=ON
cmake --build --preset release
build/release/tests/bench_vision --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
```

(On Windows the binary is `build\release\tests\bench_vision.exe`. The scene seed and every constant above are in `tests/support/stress_scene.hpp`.)