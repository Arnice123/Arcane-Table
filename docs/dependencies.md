# Dependencies

The dependency policy from design doc §10.2: every third-party library is listed here with
its version, license and reason. The license column feeds the allowlist in §10.3.

- **Source of truth for versions:** `vcpkg.json` (`builtin-baseline` pins the vcpkg catalog).
  Versions below are what baseline `bbce5bf199a8c7d91822219515cb6c937ad98e39` resolves to.
- **Qt is not in vcpkg.** It comes from the official installer (locally) or aqtinstall (CI). See ADR 0003.
- **When this table is updated:** whenever `vcpkg.json` or the baseline changes, in the same PR.

## Direct dependencies

| Library | Version | License | Why we need it | Used by | vcpkg group |
|---|---|---|---|---|---|
| Qt | 6.11.x | LGPL-3.0 (open-source edition) | GUI, widgets, networking | `client/`, `ui/` (never `core/`) | not vcpkg |
| SQLite | 3.54.1 | Public domain ("blessing") | Local campaign storage | `core/` (persistence) | core |
| Lua | 5.5.1 | MIT | Plugin scripting | `core/` (plugins) | core |
| Catch2 | 3.16.1 | BSL-1.0 | Unit tests. **Provisional**: TESTCI-01 decides | `tests/` | core |
| sol2 | 3.5.0 | MIT | C++/Lua binding candidate (spike S5) | `tools/spikes/s5_lua` | spikes |
| Protobuf | 7.36.2 | BSD-3-Clause | Wire format candidate (spike S4) | `tools/spikes/s4_wire` | spikes |
| FlatBuffers | 25.12.19 | Apache-2.0 | Wire format candidate (spike S4) | `tools/spikes/s4_wire` | spikes |
| Google Benchmark | 1.10.0 | Apache-2.0 | Spike benchmarks | `tools/spikes/*` | spikes |

## Transitive dependencies (pulled in by the above)

| Library | Version | License | Pulled in by |
|---|---|---|---|
| Abseil | 20260107.1 | Apache-2.0 | Protobuf |
| utf8_range | 7.36.2 | check (ships with Protobuf) | Protobuf |

## Notes

- **Lua 5.5 vs sol2.** The baseline gives Lua 5.5.1. vcpkg's sol2 port carries a patch for
  Lua 5.5 (based on an upstream sol2 pull request). Spike S5 must confirm it works. If it
  doesn't, pin Lua 5.4 with an override in `vcpkg.json`:
  `"overrides": [ { "name": "lua", "version": "5.4.8" } ]`.
- **Spike-only libraries** leave this table (or move to "core") when their ADR is decided.
