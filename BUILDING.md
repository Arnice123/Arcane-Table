# Building Arcane Table

From a fresh clone to a running build.

## 1. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| C++ compiler | C++20 capable | GCC 11+, Clang 14+, or MSVC 2022 |
| CMake | 3.25+ | Presets need 3.25 |
| Ninja | any recent | The presets use the Ninja generator |
| Qt | 6.5+ | Only needed for the client (`app/`, `ui/`). Widgets and OpenGLWidgets modules |

Without Qt, CMake still configures and builds the headless targets
(`core`, `net`, `server`, `tools`, `tests`). It prints a message saying the
client was skipped.

**Windows:** run the commands from a "x64 Native Tools Command Prompt for VS 2022"
so `cl` and Ninja are on the PATH.

## 2. Build

```
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

If CMake can't find Qt, point it at your Qt install:

```
cmake --preset dev-debug -DCMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
```

(Linux example: `-DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.0/gcc_64`.)

## 3. Presets

| Preset | Use it for |
|---|---|
| `dev-debug` | Everyday development |
| `dev-asan` | Debug build with AddressSanitizer and UBSan, for chasing memory bugs |
| `release` | Optimised build |
| `dev-fuzz` | libFuzzer targets (Clang only) |
| `ci-linux-gcc`, `ci-linux-clang`, `ci-windows-msvc` | What CI runs. Warnings are errors |

List them with `cmake --list-presets`. Personal tweaks go in
`CMakeUserPresets.json`, which is git-ignored.

## 4. Where things end up

Everything is under `build/<preset>/`:

- `bin/arcane-table`: the client
- `bin/arcane-server`: the headless host
- `bin/plugins/` and `bin/assets/`: copied next to the executables automatically
- `tests/`: test executables

Run `build/dev-debug/bin/arcane-server --version` to check the build.

## 5. Qt Creator

Open the root `CMakeLists.txt`, choose a Qt 6.5+ kit, and configure. Or open the
project and select one of the presets above.

## 6. The headless rule

`core/`, `net/` and `server/` must compile **without Qt or OpenGL**. This keeps the
server runnable without a GPU or display, and keeps the game logic testable. Two
checks enforce it:

- CMake fails at configure time if any of them links a Qt or OpenGL target.
- The ctest `arch.no_qt_gl_in_headless_modules` fails if a source file there
  includes a Qt or GL header.

If you hit either, move the code into `render/` or `ui/`, or put an interface in
`core` and implement it there.

## 7. Troubleshooting

| Symptom | Fix |
|---|---|
| "Qt 6.5+ not found: building the headless targets only" | Set `CMAKE_PREFIX_PATH` (see section 2), or install Qt 6.5+ |
| `cmake --preset` says the preset isn't available | Wrong host OS (the CI presets are per-OS), or CMake older than 3.25 |
| "In-source builds are not allowed" | Use a preset; builds go in `build/` |
| `dev-fuzz` fails to link with missing `libclang_rt` | Install your distro's Clang runtime package (Ubuntu: `libclang-rt-18-dev`) |
| Windows: client won't start, missing Qt DLLs | Build the `arcane_deploy_qt` target, or add Qt's `bin` folder to PATH |