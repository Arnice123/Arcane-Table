# Spike S10: proves every vcpkg dependency is found, links and runs.
# Built only with ARCANE_BUILD_SPIKES=ON (see tools/spikes/CMakeLists.txt).

arcane_find_spike_dependencies()

add_executable(deps_smoke deps_smoke.cpp)
target_compile_features(deps_smoke PRIVATE cxx_std_20)
target_link_libraries(deps_smoke PRIVATE Arcane::SQLite Arcane::Lua)

if(ARCANE_HAVE_SPIKE_DEPS)
    target_compile_definitions(deps_smoke PRIVATE ARCANE_HAVE_SPIKE_DEPS=1)
    target_link_libraries(deps_smoke PRIVATE
        sol2::sol2
        protobuf::libprotobuf
        flatbuffers::flatbuffers
        benchmark::benchmark)
endif()

# ctest runs it, so CI fails if any dependency is broken on that platform.
add_test(NAME s10.deps_smoke COMMAND deps_smoke)

# Dynamic triplets on Windows: the vcpkg DLLs must be findable when ctest runs the exe.
if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/bin")
    set_tests_properties(s10.deps_smoke PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_vcpkg_bin}")
endif()
