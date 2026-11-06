# plugin-sdk: the public surface for plugin authors. No C++ target here:
#   api-stubs/   generated Lua API stubs (editor autocomplete, Appendix E)
#   schema/      manifest and data schemas
#   templates/   starter plugins
# They are installed so the SDK can be packaged with a release.

install(DIRECTORY api-stubs schema templates
  DESTINATION share/arcane/plugin-sdk
  PATTERN ".gitkeep" EXCLUDE)
