# One smoke test per headless module. As real code lands, add files here named
# after what they test (for example dice_parser_test.cpp) and mirror the layout
# of core/src/ in subfolders.

arcane_add_test(unit.core.smoke SOURCES core_smoke.cpp LIBS arcane::core)
arcane_add_test(unit.net.smoke  SOURCES net_smoke.cpp  LIBS arcane::net arcane::core)

# Prove that the headless tests link no Qt/OpenGL, directly or transitively.
arcane_assert_no_gui_deps(test_unit_core_smoke test_unit_net_smoke)
