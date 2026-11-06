# render: OpenGL 3.3 core renderer (layers, sprite batcher, camera, textures).
# It does NOT link Qt. Qt creates the GL context in ui/ and hands it over; the
# renderer only needs a way to resolve GL function pointers, which is done with
# a loader (planned: glad, vendored in third_party/glad) fed by
# QOpenGLContext::getProcAddress. Keep Qt headers out of this module so it
# stays testable with a software GL context in CI.

arcane_add_module(render PUBLIC_DEPS arcane::core)

find_package(OpenGL QUIET)
if(TARGET OpenGL::GL)
  target_link_libraries(arcane_render PUBLIC OpenGL::GL)
else()
  message(STATUS "OpenGL development files not found: arcane_render builds without linking GL")
endif()
