# third_party: vendored dependencies that are not provided by the package
# manager (for example glad for GL loading). Every immediate subdirectory that
# contains a CMakeLists.txt is added automatically.
#
# Vendored code must not link arcane_warnings, so it stays out of our
# warnings-as-errors policy. Record each dependency's licence in the SBOM
# (story LICENSE-02).

file(GLOB children LIST_DIRECTORIES true CONFIGURE_DEPENDS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(child IN LISTS children)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt")
    add_subdirectory("${child}")
  endif()
endforeach()
