# core: entity model, command bus, persistence, rules, dice, permissions,
# scripting host, geometry. HEADLESS: no Qt, no OpenGL, no windowing.
# This is enforced at configure time (arcane_assert_no_gui_deps) and by a
# ctest (arch.no_qt_gl_in_headless_modules).

arcane_add_module(core)

# Generated header: arcane/core/version.hpp
configure_file(version.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/arcane/core/version.hpp" @ONLY)
target_include_directories(arcane_core PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")

# Threading skeleton (ARCH-03): worker pool, DB thread, I/O thread.
find_package(Threads REQUIRED)
target_link_libraries(arcane_core PUBLIC Threads::Threads)

target_include_directories(arcane_core PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${PROJECT_SOURCE_DIR}/include"
)

# Third-party libraries are added by the story that first needs them, once the
# package manager is chosen (spike S10 / ARCH-06). Planned for this module:
#   PERSIST-01  SQLite (with FTS5)
#   PLUGINS-02  Lua 5.4 + sol2
#   DICE-03     libsodium (CSPRNG)
#   ASSETS-01   BLAKE3, zstd
#   ASSETS-02   libpng, libjpeg-turbo, libwebp, an audio decoder
#   INFRA-01    spdlog
# Link them PRIVATE to arcane_core so they do not leak into other modules.
