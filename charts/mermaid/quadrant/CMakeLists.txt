cmake_minimum_required(VERSION 3.10)
project(quadrant_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/quadrant_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/quadrant_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/quadrant_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/quadrant_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/quadrant_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/quadrant_parser.y" "${LEMPAR}"
    COMMENT "Generating Quadrant C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/quadrant_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/quadrant_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/quadrant_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/quadrant_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/quadrant_parser_gen.h"
    COMMENT "Generating Quadrant C lexer with re2c"
)

add_library(quadrant STATIC
    src/quadrant_parser_wrapper.c
    src/quadrant_ast.c
    "${CMAKE_CURRENT_BINARY_DIR}/quadrant_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/quadrant_parser_gen.c"
)

target_include_directories(quadrant PUBLIC
    include
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(quadrant PUBLIC TurboParser::Parser flex_runtime)

# Test executable
add_executable(quadrant_test tests/test_parser.c)
target_link_libraries(quadrant_test quadrant Salts::TinyTest TurboParser::Parser)
set_target_properties(quadrant_test PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(quadrant_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/quadrant")

add_test(NAME quadrant_test COMMAND quadrant_test)
set_target_properties(quadrant PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(quadrant_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
