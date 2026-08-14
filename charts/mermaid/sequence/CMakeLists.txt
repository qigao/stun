cmake_minimum_required(VERSION 3.10)
project(sequence_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sequence_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/sequence_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/sequence_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/sequence_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/sequence_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sequence_parser.y" "${LEMPAR}"
    COMMENT "Generating Sequence C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sequence_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/sequence_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/sequence_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sequence_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/sequence_parser_gen.h"
    COMMENT "Generating Sequence C lexer with re2c"
)

add_library(sequence STATIC
    src/sequence_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/sequence_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/sequence_parser_gen.c"
)

target_include_directories(sequence PUBLIC
    include
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(sequence PUBLIC TurboParser::Parser)

# Test executable
add_executable(sequence_test tests/test_parser.c)
target_link_libraries(sequence_test sequence TurboUtils::TinyTest TurboParser::Parser)
target_compile_definitions(sequence_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/sequence")

add_test(NAME sequence_test COMMAND sequence_test)
set_target_properties(sequence PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(sequence_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
