# Build options and Qt discovery.

option(ARCANE_BUILD_SERVER "Build the headless dedicated server" ON)
option(ARCANE_BUILD_TOOLS "Build developer tools (asset packer, bundle CLI, schema validator)" ON)
option(ARCANE_BUILD_TESTS "Build tests (run them with ctest)" ON)
option(ARCANE_BUILD_BENCHMARKS "Build Google Benchmark targets (needs the benchmark package)" OFF)
option(ARCANE_ENABLE_FUZZING "Build libFuzzer targets (Clang only)" OFF)
option(ARCANE_WARNINGS_AS_ERRORS "Treat compiler warnings in Arcane code as errors" OFF)
option(ARCANE_WIN_CONSOLE "Keep a console window for the client on Windows (handy for logs)" ON)

set(ARCANE_SANITIZE "" CACHE STRING
  "Semicolon-separated sanitizers: address;undefined;thread (MSVC supports address only)")

set(ARCANE_QT_MIN_VERSION "6.5" CACHE STRING "Minimum Qt 6 version for the client")

set(ARCANE_BUILD_CLIENT "AUTO" CACHE STRING
  "Build render/ui/app: AUTO (only if Qt 6 is found), ON (require Qt 6), OFF")
set_property(CACHE ARCANE_BUILD_CLIENT PROPERTY STRINGS AUTO ON OFF)

string(TOUPPER "${ARCANE_BUILD_CLIENT}" _arcane_client_mode)
set(ARCANE_CLIENT_ENABLED OFF)

if(_arcane_client_mode STREQUAL "ON")
  find_package(Qt6 ${ARCANE_QT_MIN_VERSION} REQUIRED COMPONENTS Widgets OpenGLWidgets)
  set(ARCANE_CLIENT_ENABLED ON)
elseif(_arcane_client_mode STREQUAL "AUTO")
  find_package(Qt6 ${ARCANE_QT_MIN_VERSION} QUIET COMPONENTS Widgets OpenGLWidgets)
  if(Qt6_FOUND)
    set(ARCANE_CLIENT_ENABLED ON)
  else()
    message(STATUS
      "Qt ${ARCANE_QT_MIN_VERSION}+ not found: building the headless targets only "
      "(core, net, server, tools, tests). To build the client, point CMake at Qt, "
      "e.g. -DCMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64")
  endif()
elseif(_arcane_client_mode STREQUAL "OFF")
  message(STATUS "Client build disabled (ARCANE_BUILD_CLIENT=OFF)")
else()
  message(FATAL_ERROR "ARCANE_BUILD_CLIENT must be AUTO, ON or OFF (got '${ARCANE_BUILD_CLIENT}')")
endif()

if(ARCANE_CLIENT_ENABLED)
  # Qt Test is only needed for tests/ui; keep it optional.
  find_package(Qt6 ${ARCANE_QT_MIN_VERSION} QUIET OPTIONAL_COMPONENTS Test)
endif()
