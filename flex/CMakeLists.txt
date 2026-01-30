# Flex Engine - Visual Core with DSL Parser
cmake_minimum_required(VERSION 3.16)

project(flex LANGUAGES C CXX)
find_package(Stb REQUIRED)

# ============================================================================
# Parser Generation (re2c + lemon)
# ============================================================================

# Find re2c
find_program(RE2C_EXECUTABLE re2c)
if(NOT RE2C_EXECUTABLE)
  message(
    FATAL_ERROR
      "Flex: re2c not found - required for DSL parser.\n"
      "Install re2c:\n"
      "  Ubuntu/Debian: sudo apt install re2c\n"
      "  macOS: brew install re2c\n"
      "  Windows (MSYS2): pacman -S re2c\n"
      "  Or from source: https://re2c.org/")
endif()

# Find lemon (required for grammar parser)
find_program(LEMON_EXECUTABLE lemon)
if(NOT LEMON_EXECUTABLE)
  message(FATAL_ERROR "Flex: lemon not found - required for DSL parser")
endif()

message(STATUS "Flex DSL Parser generation enabled")
message(STATUS "  re2c: ${RE2C_EXECUTABLE}")
message(STATUS "  lemon: ${LEMON_EXECUTABLE}")

# Get the lempar.c template path
if(NOT LEMPAR)
  set(LEMPAR ${CMAKE_SOURCE_DIR}/vendor/lemon/lempar.c)
endif()

# Generate parser from lemon FIRST (lexer needs the generated token header)
set(FLEX_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/flex_parser.y)
set(FLEX_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.c)
set(FLEX_PARSER_GEN ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.cpp)
set(FLEX_PARSER_H ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.h)

add_custom_command(
  OUTPUT ${FLEX_PARSER_GEN} ${FLEX_PARSER_H}
  COMMAND ${CMAKE_COMMAND} -E copy ${FLEX_PARSER_Y}
          ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.y
  COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR}
          ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.y
  COMMAND ${CMAKE_COMMAND} -E rename ${FLEX_PARSER_GEN_C} ${FLEX_PARSER_GEN}
  DEPENDS ${FLEX_PARSER_Y}
  COMMENT "Generating Flex DSL parser with lemon (template: ${LEMPAR})"
  VERBATIM)

# Generate lexer from re2c (depends on parser header for token values)
set(FLEX_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/flex_lexer.re)
set(FLEX_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/flex_lexer_gen.cpp)

add_custom_command(
  OUTPUT ${FLEX_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${FLEX_LEXER_GEN} ${FLEX_LEXER_RE}
  DEPENDS ${FLEX_LEXER_RE} ${FLEX_PARSER_H}
  COMMENT "Generating Flex DSL lexer with re2c"
  VERBATIM)

# ============================================================================
# Compiler Library (re2c + lemon parser)
# ============================================================================

add_library(
  flex_compiler STATIC
  ${FLEX_LEXER_GEN} ${FLEX_PARSER_H} ${FLEX_PARSER_GEN} ${FLEX_LEXER_RE}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/flex_parser_driver.cpp)

target_include_directories(
  flex_compiler
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/ $<INSTALL_INTERFACE:include>)

set_target_properties(flex_compiler PROPERTIES FOLDER "flex"
                                               POSITION_INDEPENDENT_CODE ON)
target_link_libraries(flex_compiler PUBLIC   TurboNet::Utils)

# ============================================================================
# Runtime Library
# ============================================================================

file(GLOB_RECURSE FLEX_RUNTIME_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/*.c*)
file(GLOB_RECURSE FLEX_RUNTIME_HEADERS
     ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/runtime/*.h*)

add_library(flex_runtime STATIC ${FLEX_RUNTIME_SOURCES} ${FLEX_RUNTIME_HEADERS})

target_include_directories(
  flex_runtime
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/ $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR})

# Link physics engine (Box2D)
find_package(box2d CONFIG REQUIRED)

target_link_libraries(
  flex_runtime
  PUBLIC thorvg  TurboNet::Utils
  PRIVATE qjs box2d::box2d)

set_target_properties(flex_runtime PROPERTIES FOLDER "flex"
                                              POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Main Flex Library (combines compiler + runtime + bridge)
# ============================================================================

add_library(
  flex STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flex.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge/ast_to_runtime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge/renderer_thorvg.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge/instance_node_bridge.cpp)

target_link_libraries(flex PUBLIC flex_runtime flex_compiler thorvg)

target_include_directories(
  flex
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR})

set_target_properties(flex PROPERTIES FOLDER "flex" POSITION_INDEPENDENT_CODE
                                                    ON)

# ============================================================================
# Binary Format Library (optional)
# ============================================================================

option(FLEX_BUILD_BINARY "Build binary format library" ON)
if(FLEX_BUILD_BINARY)
  add_subdirectory(binary)
  message(STATUS "Flex: Binary format library enabled")
endif()

# ============================================================================
# Compiler Tool (optional)
# ============================================================================

option(FLEX_BUILD_TOOLS "Build compiler tools" ON)
if(FLEX_BUILD_TOOLS AND FLEX_BUILD_BINARY)
  add_subdirectory(tools)
  message(STATUS "Flex: Compiler tools enabled")
endif()

# ============================================================================
# Examples & Tests
# ============================================================================

add_subdirectory(examples)
add_subdirectory(tests)

# ============================================================================
# Install
# ============================================================================

install(
  TARGETS flex
  EXPORT flex-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/flex DESTINATION include)
