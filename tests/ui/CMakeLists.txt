# UI tests use Qt Test on the "offscreen" platform, so they run in CI without a
# display. Skipped automatically when the client is not being built.

if(ARCANE_CLIENT_ENABLED AND TARGET Qt6::Test)
  arcane_add_test(ui.smoke
    SOURCES ui_smoke.cpp
    LIBS arcane::ui Qt6::Test
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  set_target_properties(test_ui_smoke PROPERTIES AUTOMOC ON)
else()
  message(STATUS "tests/ui skipped (client disabled or Qt Test not found)")
endif()
