cmake_minimum_required(VERSION 3.14)

project(mermaid_c4)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/c4_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/c4_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/c4_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/c4_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/c4_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/c4_parser.y" "${LEMPAR}"
    COMMENT "Generating C4 parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/c4_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} --no-generation-date -o "${CMAKE_CURRENT_BINARY_DIR}/c4_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/c4_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/c4_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/c4_parser_gen.h"
    COMMENT "Generating C4 lexer with re2c"
)

add_library(mermaid_c4 STATIC
    src/c4_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/c4_parser_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/c4_lexer_gen.c"
)

target_link_libraries(mermaid_c4 PUBLIC TurboParser::Parser)

target_include_directories(mermaid_c4 PUBLIC
    include
    src
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Test executable
add_executable(c4_test
    tests/test_parser.c
)

target_link_libraries(c4_test PRIVATE mermaid_c4 Salts::TinyTest TurboParser::Parser)
target_compile_definitions(c4_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/c4")

enable_testing()
add_test(NAME c4_test COMMAND c4_test)

set_target_properties(mermaid_c4 PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(c4_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
