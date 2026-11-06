# tools: developer and pipeline command-line tools. They link core only (no Qt).
# Each one starts as a stub built from src/tool_stub.cpp. When a tool becomes
# real, give it its own folder (tools/asset-packer/...) with its own
# CMakeLists.txt and remove it from this list.

set(ARCANE_TOOL_NAMES asset-packer bundle-cli schema-validator)

foreach(tool IN LISTS ARCANE_TOOL_NAMES)
  string(REPLACE "-" "_" id "${tool}")
  add_executable(arcane_${id} src/tool_stub.cpp)
  target_compile_definitions(arcane_${id} PRIVATE ARCANE_TOOL_NAME="${tool}")
  target_link_libraries(arcane_${id} PRIVATE arcane::core arcane_warnings arcane_sanitizers)
  set_target_properties(arcane_${id} PROPERTIES OUTPUT_NAME "arcane-${tool}")
  install(TARGETS arcane_${id} RUNTIME DESTINATION bin)
endforeach()
