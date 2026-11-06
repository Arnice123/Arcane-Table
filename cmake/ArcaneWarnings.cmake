# Two INTERFACE targets that every first-party target links PRIVATE:
#   arcane_warnings    compiler warning level (+ -Werror / /WX on request)
#   arcane_sanitizers  sanitizer flags selected by ARCANE_SANITIZE
# Third-party code never links these, so vendored libraries stay quiet.

add_library(arcane_warnings INTERFACE)

if(MSVC)
  target_compile_options(arcane_warnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
  if(ARCANE_WARNINGS_AS_ERRORS)
    target_compile_options(arcane_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(arcane_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
  if(ARCANE_WARNINGS_AS_ERRORS)
    target_compile_options(arcane_warnings INTERFACE -Werror)
  endif()
endif()

add_library(arcane_sanitizers INTERFACE)

if(ARCANE_SANITIZE)
  if(MSVC)
    if("address" IN_LIST ARCANE_SANITIZE)
      target_compile_options(arcane_sanitizers INTERFACE /fsanitize=address)
      target_link_options(arcane_sanitizers INTERFACE /INCREMENTAL:NO)
      # AddressSanitizer is incompatible with /RTC1 and /ZI on MSVC.
      string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
      string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    endif()
    set(_unsupported ${ARCANE_SANITIZE})
    list(REMOVE_ITEM _unsupported address)
    if(_unsupported)
      message(WARNING "MSVC only supports the address sanitizer; ignoring: ${_unsupported}")
    endif()
  else()
    string(REPLACE ";" "," _san "${ARCANE_SANITIZE}")
    target_compile_options(arcane_sanitizers INTERFACE -fsanitize=${_san} -fno-omit-frame-pointer)
    target_link_options(arcane_sanitizers INTERFACE -fsanitize=${_san})
  endif()
endif()
