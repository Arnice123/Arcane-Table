# arcane_assert_no_gui_deps(<target>...)
#
# Walks the link dependencies of each target (transitively) and stops the
# configure step if any of them is Qt or an OpenGL/windowing library.
# Targets that do not exist (e.g. the server when ARCANE_BUILD_SERVER=OFF) are
# skipped.
function(arcane_assert_no_gui_deps)
  foreach(root IN LISTS ARGN)
    if(NOT TARGET ${root})
      continue()
    endif()

    set(queue "${root}")
    set(seen "")
    while(queue)
      list(POP_FRONT queue cur)
      if("${cur}" IN_LIST seen)
        continue()
      endif()
      list(APPEND seen "${cur}")

      if("${cur}" MATCHES "Qt[0-9]*::|Qt[0-9]+[A-Z]|OpenGL::|opengl32|GLEW|glfw|glad")
        message(FATAL_ERROR
          "Architecture violation: '${root}' depends on '${cur}'.\n"
          "core, net and server must build without Qt or OpenGL "
          "(see the design doc, sections 2 and 3). "
          "Move that code to render/ or ui/.")
      endif()

      if(TARGET "${cur}")
        set(deps "")
        foreach(prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
          get_target_property(value "${cur}" ${prop})
          if(value)
            list(APPEND deps ${value})
          endif()
        endforeach()
        foreach(dep IN LISTS deps)
          # Unwrap $<LINK_ONLY:foo>, $<BUILD_INTERFACE:foo>, ...
          string(REGEX REPLACE "^\\$<[A-Z_]+:(.*)>$" "\\1" dep "${dep}")
          list(APPEND queue "${dep}")
        endforeach()
      endif()
    endwhile()
  endforeach()
endfunction()

function(arcane_print_summary)
  if(ARCANE_CLIENT_ENABLED)
    set(client "ON (Qt ${Qt6_VERSION})")
  else()
    set(client "OFF")
  endif()
  message(STATUS "")
  message(STATUS "ArcaneTable ${PROJECT_VERSION}")
  message(STATUS "  Compiler    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
  message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
  message(STATUS "  Client      : ${client}")
  message(STATUS "  Server      : ${ARCANE_BUILD_SERVER}")
  message(STATUS "  Tools/Tests : ${ARCANE_BUILD_TOOLS} / ${ARCANE_BUILD_TESTS}")
  message(STATUS "  Sanitizers  : ${ARCANE_SANITIZE}")
  message(STATUS "  -Werror     : ${ARCANE_WARNINGS_AS_ERRORS}")
  message(STATUS "")
endfunction()
