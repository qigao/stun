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
add_dependencies(test_highlighter_loader_contract ts-lang-contract)
add_test(NAME highlighter_loader_platform_and_concurrency
  COMMAND test_highlighter_loader_contract "$<TARGET_FILE:ts-lang-contract>")
