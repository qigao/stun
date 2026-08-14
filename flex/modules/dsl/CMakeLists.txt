# Flex DSL lexer/parser frontend

if(NOT RE2C_EXECUTABLE)
  find_program(RE2C_EXECUTABLE re2c)
endif()
if(NOT RE2C_EXECUTABLE)
  message(FATAL_ERROR "Flex: re2c not found - required for DSL parser")
endif()

if(NOT LEMON_EXECUTABLE)
  find_program(LEMON_EXECUTABLE lemon)
endif()
if(NOT LEMON_EXECUTABLE)
  message(FATAL_ERROR "Flex: lemon not found - required for DSL parser")
endif()

if(NOT LEMPAR)
  if(EXISTS "${FLEX_ENGINE_ROOT}/../tools/lemon/lempar.c")
    set(LEMPAR "${FLEX_ENGINE_ROOT}/../tools/lemon/lempar.c")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/tools/lemon/lempar.c")
    set(LEMPAR "${CMAKE_SOURCE_DIR}/tools/lemon/lempar.c")
  else()
    message(FATAL_ERROR "Flex: lempar.c not found - required for DSL parser")
  endif()
endif()

set(FLEX_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/flex_parser.y)
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
  DEPENDS ${FLEX_PARSER_Y} ${LEMON_DEPENDS}
  COMMENT "Generating Flex DSL parser with lemon (template: ${LEMPAR})"
  VERBATIM)

set(FLEX_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/flex_lexer.re)
set(FLEX_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/flex_lexer_gen.cpp)
add_custom_command(
  OUTPUT ${FLEX_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${FLEX_LEXER_GEN} ${FLEX_LEXER_RE}
  DEPENDS ${FLEX_LEXER_RE} ${FLEX_PARSER_H}
  COMMENT "Generating Flex DSL lexer with re2c"
  VERBATIM)

add_library(
  flex_compiler STATIC
  ${FLEX_LEXER_GEN} ${FLEX_PARSER_H} ${FLEX_PARSER_GEN} ${FLEX_LEXER_RE}
  flex_parser_driver.cpp
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/dsl/flex_ast.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/dsl/flex_defs.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/dsl/flex_parser.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/dsl/flex_token.h)
target_include_directories(
  flex_compiler
  PUBLIC $<BUILD_INTERFACE:${FLEX_ENGINE_INCLUDE_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         $<INSTALL_INTERFACE:include>)
target_compile_features(flex_compiler PUBLIC cxx_std_17)
target_link_libraries(flex_compiler PUBLIC TurboUtils::Core Flex::Runtime)
set_target_properties(
  flex_compiler PROPERTIES FOLDER "flex/dsl" POSITION_INDEPENDENT_CODE ON)

add_library(flex_dsl INTERFACE)
target_link_libraries(flex_dsl INTERFACE flex_compiler)
set_target_properties(flex_dsl PROPERTIES FOLDER "flex/dsl")

add_library(Flex::Compiler ALIAS flex_compiler)
add_library(Flex::DSL ALIAS flex_dsl)
