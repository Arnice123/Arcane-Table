# Source-level guard: core/, net/ and server/ must not reference Qt or OpenGL.
#
# Usage:  cmake -DARCANE_ROOT=<repo root> -P cmake/CheckNoQtGl.cmake
# Exit status is non-zero when a violation is found, so it works as a CI step
# (it is also registered as a ctest, see tests/CMakeLists.txt).
#
# Comment lines are ignored. Windows, Linux and macOS all run this script the
# same way; no Python or grep is needed.

if(NOT ARCANE_ROOT)
  message(FATAL_ERROR "Pass -DARCANE_ROOT=<repo root>")
endif()

set(violations "")
set(scanned 0)

foreach(dir core net server)
  if(NOT IS_DIRECTORY "${ARCANE_ROOT}/${dir}")
    continue()
  endif()

  file(GLOB_RECURSE files "${ARCANE_ROOT}/${dir}/*")
  foreach(file IN LISTS files)
    get_filename_component(name "${file}" NAME)
    get_filename_component(ext "${file}" LAST_EXT)

    if(name STREQUAL "CMakeLists.txt" OR ext STREQUAL ".cmake")
      set(kind cmake)
    elseif(ext MATCHES "^\\.(cpp|cc|cxx|hpp|hh|hxx|h|inl|in)$")
      set(kind cpp)
    else()
      continue()
    endif()
    math(EXPR scanned "${scanned} + 1")

    file(READ "${file}" content)
    # Make the text safe to treat as a CMake list of lines.
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "," content "${content}")
    string(REPLACE "[" "(" content "${content}")
    string(REPLACE "]" ")" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")

    foreach(line IN LISTS lines)
      string(STRIP "${line}" text)
      if(text STREQUAL "")
        continue()
      endif()

      if(kind STREQUAL "cmake")
        if(text MATCHES "^#")
          continue()
        endif()
        if(text MATCHES "Qt[0-9]*::|find_package\\(.*(Qt[0-9]|OpenGL|GLEW|glfw)|qt_add_|qt_standard_project_setup|OpenGL::")
          list(APPEND violations "${dir}: ${file}: ${text}")
        endif()
      else()
        if(text MATCHES "^(//|/\\*|\\*)")
          continue()
        endif()
        if(text MATCHES "^#[ \t]*include[ \t]*[<\"](Q[A-Z]|Qt[A-Za-z]+/|GL/|GLES[0-9]*/|EGL/|OpenGL/|glad/|GLFW/)")
          list(APPEND violations "${dir}: ${file}: ${text}")
        endif()
      endif()
    endforeach()
  endforeach()
endforeach()

if(violations)
  string(REPLACE ";" "\n  " report "${violations}")
  message(FATAL_ERROR
    "core/, net/ and server/ must not reference Qt or OpenGL. Offending lines:\n  ${report}")
endif()

message(STATUS "OK: scanned ${scanned} files in core/, net/, server/ - no Qt or OpenGL references")
