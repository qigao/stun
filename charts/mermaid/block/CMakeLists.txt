cmake_minimum_required(VERSION 3.10)
project(block_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/block_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/block_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/block_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/block_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/block_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/block_parser.y" "${LEMPAR}"
    COMMENT "Generating Block C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/block_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/block_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/block_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/block_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/block_parser_gen.h"
    COMMENT "Generating Block C lexer with re2c"
)

add_library(block STATIC
    src/block_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/block_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/block_parser_gen.c"
)

target_link_libraries(block PUBLIC TurboParser::Parser flex_runtime)
target_include_directories(block PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Test executable
add_executable(block_test tests/test_parser.c)
target_link_libraries(block_test block Salts::TinyTest TurboParser::Parser)
set_target_properties(block_test PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(block_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/block")

add_test(NAME block_test COMMAND block_test)
set_target_properties(block PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(block_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
