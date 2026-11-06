# docs: documentation sources (adr/, contributor/, gm/, player/, server-admin/).
# The docs generator is chosen in story DOCS-01. Until a mkdocs.yml exists at
# the repo root this file does nothing; once it does, "arcane_docs" builds the
# site into <build>/docs-site.

find_program(ARCANE_MKDOCS mkdocs)

if(ARCANE_MKDOCS AND EXISTS "${PROJECT_SOURCE_DIR}/mkdocs.yml")
  add_custom_target(arcane_docs
    COMMAND "${ARCANE_MKDOCS}" build --strict
            -f "${PROJECT_SOURCE_DIR}/mkdocs.yml"
            -d "${CMAKE_BINARY_DIR}/docs-site"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building documentation site"
    VERBATIM)
endif()
