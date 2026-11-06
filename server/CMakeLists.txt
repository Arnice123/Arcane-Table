# server: headless dedicated-server executable. Links core + net ONLY.
# Linking render/ui/Qt here would break the "runs on a box with no display"
# requirement, and arcane_assert_no_gui_deps() would stop the configure step.

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(arcane_server ${sources})
target_link_libraries(arcane_server PRIVATE
  arcane::net
  arcane::core
  arcane_warnings
  arcane_sanitizers)
set_target_properties(arcane_server PROPERTIES OUTPUT_NAME "arcane-server")

install(TARGETS arcane_server RUNTIME DESTINATION bin)
