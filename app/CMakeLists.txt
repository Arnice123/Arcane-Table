# app: the client executable. Links everything (ui, render, net, core, Qt).

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(arcane_table ${sources})
target_link_libraries(arcane_table PRIVATE
  arcane::ui
  arcane::render
  arcane::net
  arcane::core
  Qt6::Widgets
  arcane_warnings
  arcane_sanitizers)
set_target_properties(arcane_table PROPERTIES
  OUTPUT_NAME "arcane-table"
  MACOSX_BUNDLE ON)

if(WIN32 AND NOT ARCANE_WIN_CONSOLE)
  set_target_properties(arcane_table PROPERTIES WIN32_EXECUTABLE ON)
endif()

install(TARGETS arcane_table
  RUNTIME DESTINATION bin
  BUNDLE DESTINATION .)

# Windows: running the .exe outside Qt Creator needs the Qt DLLs next to it.
#   cmake --build <dir> --target arcane_deploy_qt
if(WIN32)
  find_program(ARCANE_WINDEPLOYQT NAMES windeployqt windeployqt6
    HINTS "${Qt6_DIR}/../../../bin")
  if(ARCANE_WINDEPLOYQT)
    add_custom_target(arcane_deploy_qt
      COMMAND "${ARCANE_WINDEPLOYQT}" --no-translations "$<TARGET_FILE:arcane_table>"
      COMMENT "Copying Qt runtime next to arcane-table.exe"
      VERBATIM)
    add_dependencies(arcane_deploy_qt arcane_table)
  endif()
endif()
