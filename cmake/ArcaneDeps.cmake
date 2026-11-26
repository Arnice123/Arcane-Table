# ArcaneDeps.cmake
# The one place where third-party dependencies are found.
# Every target links the Arcane::* names below, never vcpkg's own target names,
# so swapping the dependency manager later only touches this file.
#
# Qt is deliberately NOT found here. It comes from the official installer
# (locally) or aqtinstall (CI) and is found by the modules that need it, via
# CMAKE_PREFIX_PATH. See ADR 0003.

include_guard(GLOBAL)

# Core dependencies: listed under "dependencies" in vcpkg.json.
function(arcane_find_core_dependencies)
    # SQLite: vcpkg ships a CMake config package.
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    if(NOT TARGET Arcane::SQLite)
        add_library(Arcane::SQLite ALIAS unofficial::sqlite3::sqlite3)
    endif()

    # Lua: the vcpkg port uses CMake's FindLua module, which gives variables,
    # not a target. Wrap them in a target so the rest of the tree stays clean.
    find_package(Lua REQUIRED)
    if(NOT TARGET arcane_lua)
        add_library(arcane_lua INTERFACE)
        target_include_directories(arcane_lua SYSTEM INTERFACE ${LUA_INCLUDE_DIR})
        target_link_libraries(arcane_lua INTERFACE ${LUA_LIBRARIES})
        add_library(Arcane::Lua ALIAS arcane_lua)
    endif()
    set(ARCANE_LUA_VERSION "${LUA_VERSION_STRING}" PARENT_SCOPE)

    # Test framework: PROVISIONAL choice (story TESTCI-01 decides).
    find_package(Catch2 3 CONFIG REQUIRED)
endfunction()

# Spike-only dependencies: the "spikes" feature in vcpkg.json.
# Turn it on with VCPKG_MANIFEST_FEATURES=spikes (the *-spikes presets do this).
# Sets ARCANE_HAVE_SPIKE_DEPS in the caller's scope.
function(arcane_find_spike_dependencies)
    find_package(sol2 CONFIG QUIET)
    find_package(Protobuf CONFIG QUIET)
    find_package(flatbuffers CONFIG QUIET)
    find_package(benchmark CONFIG QUIET)

    if(sol2_FOUND AND Protobuf_FOUND AND flatbuffers_FOUND AND benchmark_FOUND)
        set(ARCANE_HAVE_SPIKE_DEPS ON PARENT_SCOPE)
    else()
        set(ARCANE_HAVE_SPIKE_DEPS OFF PARENT_SCOPE)
        message(STATUS "ArcaneDeps: spike libraries not installed "
                       "(configure with VCPKG_MANIFEST_FEATURES=spikes to get them)")
    endif()
endfunction()
