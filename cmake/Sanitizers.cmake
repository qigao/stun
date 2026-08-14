# Sanitizer options are directory-wide so every repository C/C++ object that
# participates in a test executable uses the same instrumentation contract.
option(ENABLE_SANITIZER_ADDRESS "Enable AddressSanitizer" OFF)
option(ENABLE_SANITIZER_UNDEFINED "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_SANITIZER_LEAK "Enable LeakSanitizer" OFF)
option(ENABLE_SANITIZER_THREAD "Enable ThreadSanitizer" OFF)
option(ENABLE_SANITIZER_MEMORY "Enable MemorySanitizer (Clang only)" OFF)

set(_sanitizer_enabled FALSE)
foreach(_sanitizer_option IN ITEMS
        ENABLE_SANITIZER_ADDRESS
        ENABLE_SANITIZER_UNDEFINED
        ENABLE_SANITIZER_LEAK
        ENABLE_SANITIZER_THREAD
        ENABLE_SANITIZER_MEMORY)
  if(${_sanitizer_option})
    set(_sanitizer_enabled TRUE)
  endif()
endforeach()

if(NOT _sanitizer_enabled)
  return()
endif()

if(ENABLE_SANITIZER_THREAD AND
   (ENABLE_SANITIZER_ADDRESS OR ENABLE_SANITIZER_LEAK OR
    ENABLE_SANITIZER_MEMORY))
  message(FATAL_ERROR
    "ThreadSanitizer cannot be combined with AddressSanitizer, "
    "LeakSanitizer, or MemorySanitizer")
endif()

if(ENABLE_SANITIZER_MEMORY AND
   (ENABLE_SANITIZER_ADDRESS OR ENABLE_SANITIZER_UNDEFINED OR
    ENABLE_SANITIZER_LEAK OR ENABLE_SANITIZER_THREAD))
  message(FATAL_ERROR "MemorySanitizer cannot be combined with other sanitizers")
endif()

message(STATUS "=== Sanitizers Configuration ===")
message(STATUS "AddressSanitizer: ${ENABLE_SANITIZER_ADDRESS}")
message(STATUS "UndefinedBehaviorSanitizer: ${ENABLE_SANITIZER_UNDEFINED}")
message(STATUS "LeakSanitizer: ${ENABLE_SANITIZER_LEAK}")
message(STATUS "ThreadSanitizer: ${ENABLE_SANITIZER_THREAD}")
message(STATUS "MemorySanitizer: ${ENABLE_SANITIZER_MEMORY}")
message(STATUS "================================")

if(MSVC)
  if(ENABLE_SANITIZER_UNDEFINED OR ENABLE_SANITIZER_LEAK OR
     ENABLE_SANITIZER_THREAD OR ENABLE_SANITIZER_MEMORY)
    message(FATAL_ERROR
      "MSVC supports ENABLE_SANITIZER_ADDRESS only in this project")
  endif()

  # MSVC ASan is incompatible with CMake's default Debug /RTC1 and with
  # incremental linking. Remove the former at its cache source and override
  # the latter for executable/shared/module link steps.
  foreach(_flags_var IN ITEMS CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
    string(REGEX REPLACE "(^| )[/-]RTC[^ ]*" ""
           _flags_without_rtc "${${_flags_var}}")
    string(STRIP "${_flags_without_rtc}" _flags_without_rtc)
    set(${_flags_var} "${_flags_without_rtc}" CACHE STRING
        "Flags used by the ${_flags_var} configuration" FORCE)
  endforeach()

  add_compile_options(
    "$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<BOOL:$<TARGET_PROPERTY:NANOGUI_DISABLE_ADDRESS_SANITIZER>>>>:/fsanitize=address>")
  add_link_options(/INCREMENTAL:NO)
  return()
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  message(FATAL_ERROR
    "Sanitizers are unsupported with ${CMAKE_CXX_COMPILER_ID}")
endif()

if(ENABLE_SANITIZER_MEMORY AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "MemorySanitizer requires Clang")
endif()

set(_sanitizer_compile_flags)
set(_sanitizer_link_flags)
if(ENABLE_SANITIZER_ADDRESS)
  list(APPEND _sanitizer_compile_flags -fsanitize=address)
  list(APPEND _sanitizer_link_flags -fsanitize=address)
endif()
if(ENABLE_SANITIZER_UNDEFINED)
  list(APPEND _sanitizer_compile_flags -fsanitize=undefined)
  list(APPEND _sanitizer_link_flags -fsanitize=undefined)
endif()
if(ENABLE_SANITIZER_LEAK)
  list(APPEND _sanitizer_compile_flags -fsanitize=leak)
  list(APPEND _sanitizer_link_flags -fsanitize=leak)
endif()
if(ENABLE_SANITIZER_THREAD)
  list(APPEND _sanitizer_compile_flags -fsanitize=thread)
  list(APPEND _sanitizer_link_flags -fsanitize=thread)
endif()
if(ENABLE_SANITIZER_MEMORY)
  list(APPEND _sanitizer_compile_flags
       -fsanitize=memory
       -fsanitize-memory-track-origins)
  list(APPEND _sanitizer_link_flags -fsanitize=memory)
endif()

foreach(_sanitizer_flag IN LISTS _sanitizer_compile_flags)
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX>:${_sanitizer_flag}>")
endforeach()
add_compile_options(
  "$<$<COMPILE_LANGUAGE:C,CXX>:-fno-omit-frame-pointer>"
  "$<$<COMPILE_LANGUAGE:C,CXX>:-fno-optimize-sibling-calls>")
add_link_options(${_sanitizer_link_flags})
