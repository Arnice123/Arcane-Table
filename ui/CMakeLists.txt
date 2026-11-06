# ui: Qt Widgets layer (shell, docking, theme, panels, tools, wizards).
# Only built when the client is enabled (Qt found). Depends on core + render.

arcane_add_module(ui
  PUBLIC_DEPS
    arcane::core
    arcane::render
    Qt6::Widgets
    Qt6::OpenGLWidgets)

# Q_OBJECT classes anywhere under include/ or src/ are picked up by moc.
set_target_properties(arcane_ui PROPERTIES AUTOMOC ON)
