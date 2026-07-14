# DotGraph Module - DOT language parser + renderer
cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Parser Generation (re2c + lemon)
# ============================================================================

set(DG_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/dotgraph_parser.y)
set(DG_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/dotgraph_parser_gen.c)
set(DG_PARSER_GEN_H ${CMAKE_CURRENT_BINARY_DIR}/dotgraph_parser_gen.h)

add_custom_command(
    OUTPUT ${DG_PARSER_GEN_C} ${DG_PARSER_GEN_H}
    COMMAND ${CMAKE_COMMAND} -E copy ${DG_PARSER_Y} ${CMAKE_CURRENT_BINARY_DIR}/dotgraph_parser_gen.y
    COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR} ${CMAKE_CURRENT_BINARY_DIR}/dotgraph_parser_gen.y
    DEPENDS ${DG_PARSER_Y} ${LEMPAR}
    COMMENT "Generating DotGraph parser with lemon"
    VERBATIM
)

set(DG_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/dotgraph_lexer.re)
set(DG_LEXER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/dotgraph_lexer_gen.c)

add_custom_command(
    OUTPUT ${DG_LEXER_GEN_C}
    COMMAND ${RE2C_EXECUTABLE} -o ${DG_LEXER_GEN_C} ${DG_LEXER_RE}
    DEPENDS ${DG_LEXER_RE} ${DG_PARSER_GEN_H}
    COMMENT "Generating DotGraph lexer with re2c"
    VERBATIM
)

# ============================================================================
# DotGraph Parser Library
# ============================================================================

add_library(dotgraph_parser STATIC
    ${DG_LEXER_GEN_C}
    ${DG_PARSER_GEN_C}
    src/dotgraph_parser_wrapper.c
)

target_include_directories(dotgraph_parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        src
)

target_link_libraries(dotgraph_parser PUBLIC TurboUtils::Parser)

# ============================================================================
# Backend-neutral layout + SVG export
# ============================================================================

add_library(dotgraph STATIC
    src/dotgraph_renderer.cpp
)

target_include_directories(dotgraph
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        src
)

target_link_libraries(dotgraph
    PUBLIC dotgraph_parser
    PRIVATE flex_expression TurboUtils::Mustache avoid
)

# ============================================================================
# Flex component adapter
# ============================================================================

add_library(flex_dotgraph_adapter STATIC
    src/flex_dotgraph.cpp
    src/flexui_dotgraph.cpp
)
target_include_directories(flex_dotgraph_adapter
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(flex_dotgraph_adapter PUBLIC dotgraph flexUI)

if(WIN32)
    target_compile_definitions(dotgraph_parser PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(dotgraph PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(flex_dotgraph_adapter PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set_target_properties(dotgraph_parser PROPERTIES FOLDER "Modules/DotGraph")
set_target_properties(dotgraph PROPERTIES FOLDER "Modules/DotGraph")
set_target_properties(flex_dotgraph_adapter PROPERTIES FOLDER "Modules/DotGraph")

# ============================================================================
# Tests
# ============================================================================

add_executable(dotgraph_parser_test tests/dotgraph_parser_tests.cpp)
target_link_libraries(dotgraph_parser_test PRIVATE dotgraph flex_dotgraph_adapter TurboUtils::TinyTest)
add_test(NAME DotGraphParserTest COMMAND dotgraph_parser_test)
set_target_properties(dotgraph_parser_test PROPERTIES FOLDER "Modules/DotGraph/Tests")
