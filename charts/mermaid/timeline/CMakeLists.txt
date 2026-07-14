cmake_minimum_required(VERSION 3.10)
project(timeline_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timeline_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/timeline_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/timeline_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/timeline_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/timeline_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/timeline_parser.y" "${LEMPAR}"
    COMMENT "Generating Timeline C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timeline_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/timeline_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/timeline_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/timeline_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/timeline_parser_gen.h"
    COMMENT "Generating Timeline C lexer with re2c"
)

add_library(timeline STATIC
    src/timeline_parser_wrapper.c
    src/timeline_ast.c
    "${CMAKE_CURRENT_BINARY_DIR}/timeline_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/timeline_parser_gen.c"
)

target_include_directories(timeline PUBLIC
    include
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(timeline PUBLIC TurboUtils::Parser)

# Test executable
add_executable(timeline_test tests/test_parser.c)
target_link_libraries(timeline_test timeline TurboUtils::TinyTest)
target_compile_definitions(timeline_test PRIVATE
    GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/timeline"
)

add_test(NAME timeline_test COMMAND timeline_test)
set_target_properties(timeline PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(timeline_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
