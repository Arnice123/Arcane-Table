# Spike S2 (ARCH-05): Qt overlay widgets vs a HUD drawn in GL.
# Output: build/<preset>/bin/arcane-spike-s2-hud
#
# Sources are globbed, so adding spike_canvas.cpp, qt_hud_overlay.cpp, ... only
# needs a CMake re-run (done automatically on rebuild via CONFIGURE_DEPENDS).
# Resources (pixel-art test textures, a .qrc file) are picked up by AUTORCC.

# OpenGL is a dependency of OpenGLWidgets; named explicitly because the spike
# uses the shader/buffer helpers and QOpenGLFunctions_3_3_Core from Qt6::OpenGL.
cmake_minimum_required(VERSION 3.30)

find_package(Qt6 ${ARCANE_QT_MIN_VERSION} REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

file(GLOB sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc")

if(NOT sources)
  message(FATAL_ERROR "tools/spikes/s2_hud has no source files")
endif()

if(NOT TARGET arcane_sanitizers)
  message(FATAL_ERROR "arcane_sanitizers target does not exist")
endif()

add_executable(arcane_spike_s2_hud ${sources})

target_link_libraries(arcane_spike_s2_hud PRIVATE
  Qt6::Widgets
  Qt6::OpenGL
  Qt6::OpenGLWidgets
  arcane_sanitizers)          # sanitizers allowed; warnings-as-errors deliberately not

target_include_directories(arcane_spike_s2_hud PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arcane_spike_s2_hud PRIVATE cxx_std_20)

set_target_properties(arcane_spike_s2_hud PROPERTIES
  OUTPUT_NAME "arcane-spike-s2-hud"
  AUTOMOC ON
  AUTORCC ON)

# Keep a console on Windows so qDebug() output and your timing lines are visible.
if(WIN32 AND NOT ARCANE_WIN_CONSOLE)
  set_target_properties(arcane_spike_s2_hud PROPERTIES WIN32_EXECUTABLE ON)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})

# Windows: running the .exe outside Qt Creator needs the Qt DLLs next to it.
#   cmake --build <dir> --target arcane_spike_s2_deploy_qt
if(WIN32)
  find_program(ARCANE_WINDEPLOYQT NAMES windeployqt windeployqt6
    HINTS "${Qt6_DIR}/../../../bin")
  if(ARCANE_WINDEPLOYQT)
    add_custom_target(arcane_spike_s2_deploy_qt
      COMMAND "${ARCANE_WINDEPLOYQT}" --no-translations "$<TARGET_FILE:arcane_spike_s2_hud>"
      COMMENT "Copying Qt runtime next to arcane-spike-s2-hud.exe"
      VERBATIM)
    add_dependencies(arcane_spike_s2_deploy_qt arcane_spike_s2_hud)
  endif()
endif()
