# Helpers used by the per-folder CMakeLists.txt files.

# arcane_add_module(<name> [PUBLIC_DEPS ...] [PRIVATE_DEPS ...])
#
# Creates the static library arcane_<name> (alias arcane::<name>) from
#   <module>/src/**/*.cpp|*.hpp   and   <module>/include/**/*.hpp
# Public headers live in <module>/include/arcane/<name>/.
# Sources are globbed with CONFIGURE_DEPENDS, so adding a file under src/ does
# not require editing any CMakeLists.txt: just rebuild.
function(arcane_add_module name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "PUBLIC_DEPS;PRIVATE_DEPS")

  set(target "arcane_${name}")
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

  add_library(${target} STATIC ${sources})
  add_library(arcane::${name} ALIAS ${target})

  target_include_directories(${target}
    PUBLIC  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_link_libraries(${target}
    PUBLIC  ${ARG_PUBLIC_DEPS}
    PRIVATE ${ARG_PRIVATE_DEPS} arcane_warnings arcane_sanitizers)

  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})
endfunction()

# arcane_stage_directory(<target> <source_dir> <dest_subdir>)
#
# Adds an ALL target that copies <source_dir> to <exe dir>/<dest_subdir> so the
# built executables can find plugins/ and assets/ next to themselves.
function(arcane_stage_directory target source_dir dest_subdir)
  if(TARGET arcane_server)
    set(dest "$<TARGET_FILE_DIR:arcane_server>/${dest_subdir}")
  elseif(TARGET arcane_table)
    set(dest "$<TARGET_FILE_DIR:arcane_table>/${dest_subdir}")
  else()
    set(dest "${CMAKE_BINARY_DIR}/bin/${dest_subdir}")
  endif()

  add_custom_target(${target} ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}" "${dest}"
    COMMENT "Staging ${dest_subdir}/"
    VERBATIM)
endfunction()

# arcane_stage_subdirectories(<prefix> <source_root> <dest_subdir> <install_dest>)
#
# Stages and installs every immediate subdirectory of <source_root> (the
# CMakeLists.txt that sits next to them is deliberately not copied).
function(arcane_stage_subdirectories prefix source_root dest_subdir install_dest)
  file(GLOB children LIST_DIRECTORIES true CONFIGURE_DEPENDS
    RELATIVE "${source_root}" "${source_root}/*")

  foreach(child IN LISTS children)
    if(NOT IS_DIRECTORY "${source_root}/${child}")
      continue()
    endif()
    arcane_stage_directory(${prefix}_${child}
      "${source_root}/${child}" "${dest_subdir}/${child}")
    install(DIRECTORY "${source_root}/${child}"
      DESTINATION "${install_dest}"
      PATTERN ".gitkeep" EXCLUDE)
  endforeach()
endfunction()
