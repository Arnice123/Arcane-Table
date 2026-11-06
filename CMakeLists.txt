cmake_minimum_required(VERSION 3.25)

project(ArcaneTable
  VERSION 0.1.0
  DESCRIPTION "System-agnostic virtual tabletop engine"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not supported. Use a preset (cmake --preset dev-debug) "
    "or point CMake at a separate build directory.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Everything lands in <build>/bin so the executables, staged plugins/ and
# staged assets/ end up side by side (single-config generators).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

include(ArcaneOptions)
include(ArcaneWarnings)
include(ArcaneModule)
include(ArcaneChecks)

if(ARCANE_BUILD_TESTS)
  enable_testing()
endif()

# --- Dependency order: lower layers first --------------------------------
#
#   app --> ui --> render --> core
#    |       |                  ^
#    +-------+--> net ---------+
#   server --> net --> core
#
add_subdirectory(third_party)
add_subdirectory(core)
add_subdirectory(net)

if(ARCANE_BUILD_SERVER)
  add_subdirectory(server)
endif()

if(ARCANE_CLIENT_ENABLED)
  add_subdirectory(render)
  add_subdirectory(ui)
  add_subdirectory(app)
endif()

add_subdirectory(plugin-sdk)
add_subdirectory(plugins-builtin)
add_subdirectory(assets)

if(ARCANE_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(ARCANE_BUILD_TESTS)
  add_subdirectory(tests)
endif()

add_subdirectory(docs)

# --- Architecture guard ---------------------------------------------------
# core, net and server must never depend on Qt or OpenGL (design doc 2 / 3).
# This fails the *configure* step if any of them (or anything they link) does.
# A source-level scan runs as a ctest too (tests/CMakeLists.txt).
arcane_assert_no_gui_deps(arcane_core arcane_net arcane_server)

arcane_print_summary()
