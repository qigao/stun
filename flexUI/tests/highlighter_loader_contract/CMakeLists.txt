cmake_minimum_required(VERSION 3.20)
project(stun_highlighter_loader_contract LANGUAGES C CXX)
enable_testing()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

# Standalone provider: no Salts, FlexUI runtime or downloaded grammar.
# CMake selects .dll / .dylib / .so on the host platform.
add_library(ts-lang-contract SHARED plugin_contract.c)
set_target_properties(ts-lang-contract PROPERTIES
  PREFIX "" OUTPUT_NAME "ts-lang-contract")
target_include_directories(ts-lang-contract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

add_executable(test_highlighter_loader_contract
  test_loader_contract.cpp
  ../../src/highlighters/ts_plugin_loader.cpp
  ../../src/highlighters/treesitter_highlighter.cpp
  ../../src/syntax_highlighter.cpp)
target_include_directories(test_highlighter_loader_contract PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_link_libraries(test_highlighter_loader_contract PRIVATE
  Threads::Threads ${CMAKE_DL_LIBS})
# Isolated Linux TSan gate instruments BOTH the executable and the loaded
# C11 provider. It never brings the full FlexUI/Salts runtime into this test.
option(STUN_HIGHLIGHTER_TSAN "Enable isolated C11/C++17 loader ThreadSanitizer" OFF)
if(STUN_HIGHLIGHTER_TSAN)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
     NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR "Highlighter TSan gate needs Linux GCC/Clang")
  endif()
  foreach(target IN ITEMS ts-lang-contract test_highlighter_loader_contract)
    target_compile_options(${target} PRIVATE
      -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=thread)
  endforeach()
  # Work around Linux TSan address-space collisions with executable PIE.
  # Do not relax TSan diagnostics, warnings or the actual concurrency test.
  target_link_options(test_highlighter_loader_contract PRIVATE -no-pie)
endif()

add_dependencies(test_highlighter_loader_contract ts-lang-contract)
add_test(NAME highlighter_loader_platform_and_concurrency
  COMMAND test_highlighter_loader_contract "$<TARGET_FILE:ts-lang-contract>")
