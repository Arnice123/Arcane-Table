# Integration tests: host + N clients over the network simulator (story
# NET-06), database round trips, plugin harness. Headless, no Qt.

arcane_add_test(integration.smoke SOURCES smoke.cpp LIBS arcane::net arcane::core)
