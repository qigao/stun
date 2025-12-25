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
set(FLEX_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/flex_parser.y)
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
set(FLEX_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/flex_lexer.re)
set(FLEX_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/flex_lexer_gen.cpp)

add_custom_command(
  OUTPUT ${FLEX_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${FLEX_LEXER_GEN} ${FLEX_LEXER_RE}
  DEPENDS ${FLEX_LEXER_RE} ${FLEX_PARSER_H}
  COMMENT "Generating Flex DSL lexer with re2c"
  VERBATIM)

# ============================================================================
# Library
# ============================================================================
file(GLOB_RECURSE FLEX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c*)
file(GLOB_RECURSE FLEX_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h*)

add_library(flex STATIC ${FLEX_SOURCES} ${FLEX_HEADERS} ${FLEX_LEXER_GEN}
                        ${FLEX_PARSER_GEN} ${FLEX_PARSER_Y} ${FLEX_LEXER_RE})

target_include_directories(
  flex
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         $<INSTALL_INTERFACE:include> ${Stb_INCLUDE_DIR})

# Link physics engine (Box2D)
find_package(box2d CONFIG REQUIRED) 
# Link rendering backend (ThorVG)
target_link_libraries(
  flex
  PUBLIC thorvg TurboMemory fmtlog  
  PRIVATE qjs box2d::box2d)

set_target_properties(flex PROPERTIES FOLDER "flex" POSITION_INDEPENDENT_CODE
                                                    ON)

# ============================================================================
# Widgets Library
# ============================================================================

add_subdirectory(widgets)

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
