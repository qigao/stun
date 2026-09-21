cmake_minimum_required(VERSION 3.10)
project(sankey_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sankey_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/sankey_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/sankey_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/sankey_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/sankey_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sankey_parser.y" "${LEMPAR}"
    COMMENT "Generating Sankey C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sankey_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/sankey_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/sankey_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sankey_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/sankey_parser_gen.h"
    COMMENT "Generating Sankey C lexer with re2c"
)

add_library(sankey STATIC
    src/sankey_parser_wrapper.c
    src/sankey_ast.c
    "${CMAKE_CURRENT_BINARY_DIR}/sankey_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/sankey_parser_gen.c"
)

target_include_directories(sankey PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(sankey PUBLIC TurboParser::Parser flex_runtime)

# Test executable
add_executable(sankey_test tests/test_parser.c)
target_link_libraries(sankey_test sankey Salts::TinyTest)
set_target_properties(sankey_test PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(sankey_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/sankey")

add_test(NAME sankey_test COMMAND sankey_test)
set_target_properties(sankey PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(sankey_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
