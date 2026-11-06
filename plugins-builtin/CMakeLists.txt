# plugins-builtin: first-party rulesets and modules shipped with the engine
# (generic ruleset, SRD demo ruleset, ...). Each immediate subdirectory is one
# plugin. They are data + Lua only, loaded through the same plugin API as
# third-party plugins; never special-case them in the engine.
#
# Each plugin directory is copied to <exe dir>/plugins/<name>/ at build time
# and installed to share/arcane/plugins/<name>/.

arcane_stage_subdirectories(arcane_stage_plugins
  "${CMAKE_CURRENT_SOURCE_DIR}" plugins share/arcane/plugins)
