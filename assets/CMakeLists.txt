# assets: art, audio, fonts, icons and theme files that ship with the engine
# (tokens/, tiles/, ui/, icons/, fonts/, audio/). Distinct from core/src/assets,
# which is the import and storage code.
#
# Each immediate subdirectory is copied to <exe dir>/assets/<name>/ at build
# time and installed to share/arcane/assets/<name>/. Keep credits and licences
# for everything here in ASSET_CREDITS.md at the repo root of this folder.

arcane_stage_subdirectories(arcane_stage_assets
  "${CMAKE_CURRENT_SOURCE_DIR}" assets share/arcane/assets)
