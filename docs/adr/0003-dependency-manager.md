# ADR 0003: Dependency manager and where Qt comes from

- **Status:** Proposed
- **Date:** 2026-11-14
- **Story:** ARCH-06, spike S10
- **Decision log:** D-01

## Context

Until now every library was installed by hand on the developer's machine. That fails for a second
contributor and for CI, where every run starts on a blank machine. The project needs a file that
says which libraries, at which versions, are needed, and a tool that fetches and builds them for
anyone, on every compiler the team uses (MinGW 64-bit, MSVC, GCC).

Qt is a special case: it is large, and building it from source is slow. Design doc §10.2 already
says Qt comes from the official installer locally and from aqtinstall in CI.

## Options

1. **vcpkg, manifest mode, for everything except Qt.** (The design doc's lean.)
2. **vcpkg for everything, Qt included.**
3. **Conan.** Not trialled; see "Options not evaluated".

### Options not evaluated

Conan was not trialled in this spike. It is a mature, capable tool. The decision below rests on
vcpkg meeting the requirements and on the design doc already leaning to it (D-01), not on a
comparison. Revisit only if vcpkg fails a requirement.

## Method

- `vcpkg.json` at the repo root, `builtin-baseline` = `bbce5bf199a8c7d91822219515cb6c937ad98e39`.
  Core dependencies: `sqlite3`, `lua`, `catch2`. A `spikes` feature: `sol2`, `protobuf`,
  `flatbuffers`, `benchmark`.
- CMake presets set the vcpkg toolchain file (applied before `project()`) and a triplet per
  compiler: `x64-linux`, `x64-mingw-dynamic` (target and host), `x64-windows`.
- `cmake/ArcaneDeps.cmake` is the only place that calls `find_package` for third-party
  libraries; everything else links `Arcane::*` targets.
- `tools/spikes/s10_deps/deps_smoke` prints each library's version and runs a trivial call;
  ctest runs it on every CI job.
- GitHub Actions: Ubuntu 24.04 GCC, Windows MinGW (Qt's own MinGW), Windows MSVC (optional).
  vcpkg is checked out at the baseline commit. Qt via `jurplel/install-qt-action` (aqtinstall).
- Binary cache: vcpkg `files` provider in a folder, saved and restored with `actions/cache`.
  (vcpkg's GitHub Actions cache backend `x-gha` has been removed from vcpkg, so it was not an option.)
- Cold run = all `vcpkg-*` caches deleted first. Warm run = immediate re-run with no changes.

Tested versions: vcpkg tool `________`, CMake `________`, Qt `________`,
GCC `________`, MinGW `________`, MSVC `________`.

## Results

### CI (from each job's run summary)

| Job | Cold total | Warm total | vcpkg install, cold | vcpkg install, warm | Qt provisioning | Cache size |
|---|---|---|---|---|---|---|
| linux-gcc | | | | | | |
| windows-mingw | | | | | | |
| windows-msvc | | | | | | |

GitHub's cache storage limit per repository: `____` (check current GitHub docs).
Total of our caches: `____`.

### Local

| Measure | Value |
|---|---|
| Clean machine to green build (newcomer target: 30 min) | |
| ...of which Qt install | |
| ...of which vcpkg first install | |

### Qt through vcpkg (checkpoint C7)

| Measure | Value |
|---|---|
| Ports requested | `qtbase` (+ modules: ____ ) |
| Result | finished in ___ min / stopped at 45 min |
| Machine | CPU ___, cores ___, RAM ___ |

## Analysis

<!-- Fill in from the numbers. Questions to answer:
- Is the warm CI time acceptable for every PR?
- Does the cold time matter (it happens after every baseline bump or vcpkg.json change)?
- Will the caches fit inside GitHub's limit with three jobs?
- Does the newcomer path fit in 30 minutes?
- Does the C7 number justify keeping Qt outside vcpkg? -->

## Decision

1. **Dependency manager:** vcpkg in manifest mode, with a pinned `builtin-baseline`.
2. **Qt:** not from vcpkg. The official installer locally, aqtinstall in CI. Qt's MinGW is used for
   the MinGW triplet so Qt and the vcpkg libraries share a compiler.