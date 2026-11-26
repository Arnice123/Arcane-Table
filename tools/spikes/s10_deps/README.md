# Spike S10: running it locally

One-time setup:

```sh
git clone https://github.com/microsoft/vcpkg.git C:/dev/vcpkg   # any folder outside the repo
C:/dev/vcpkg/bootstrap-vcpkg.bat -disableMetrics                 # bootstrap-vcpkg.sh on Linux
```

Set two environment variables (user-level, then restart your shell/IDE):

| Variable | Example (Windows MinGW) | Example (Linux) |
|---|---|---|
| `VCPKG_ROOT` | `C:/dev/vcpkg` | `~/dev/vcpkg` |
| `QT_ROOT_DIR` | `C:/Qt/6.11.0/mingw_64` | `~/Qt/6.11.0/gcc_64` |

On Windows MinGW, Qt's MinGW (`C:/Qt/Tools/mingw1310_64/bin`, or whichever your Qt shipped)
must be first on `PATH`, so vcpkg builds the libraries with the same compiler as Qt.

| Checkpoint | Command | Expect |
|---|---|---|
| C1 | `cmake --preset dev-mingw-debug` | vcpkg builds `sqlite3`, `lua`, `catch2` |
| C2 | `cmake --preset dev-mingw-spikes` then `cmake --build --preset dev-mingw-spikes` then run `build/dev-mingw-spikes/tools/spikes/s10_deps/deps_smoke` | Both versions print, "all checks passed" |
| C3 | (same preset as C2, which turns on the `spikes` feature) | Protobuf, FlatBuffers, sol2 versions print |
| C7 | `"$VCPKG_ROOT/vcpkg" install qtbase --triplet x64-mingw-dynamic --host-triplet x64-mingw-dynamic --x-install-root=C:/tmp/qt-trial` | Stop at about 45 minutes; record the result in ADR 0003 |

On Linux use `dev-debug` / `spikes-release` instead of the `dev-mingw-*` presets.

If configure fails, read `build/<preset>/vcpkg-manifest-install.log` first.
