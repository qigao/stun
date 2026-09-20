cmake_minimum_required(VERSION 3.10)
project(statediagram_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/statediagram_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/statediagram_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/statediagram_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/statediagram_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/statediagram_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/statediagram_parser.y" "${LEMPAR}"
    COMMENT "Generating StateDiagram C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/statediagram_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/statediagram_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/statediagram_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/statediagram_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/statediagram_parser_gen.h"
    COMMENT "Generating StateDiagram C lexer with re2c"
)

add_library(statediagram STATIC
    src/statediagram_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/statediagram_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/statediagram_parser_gen.c"
)

target_include_directories(statediagram PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(statediagram PUBLIC TurboParser::Parser)
# Test executable
add_executable(statediagram_test tests/test_parser.c)
target_link_libraries(statediagram_test statediagram Salts::TinyTest TurboParser::Parser)
target_compile_definitions(statediagram_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/state")

add_test(NAME statediagram_test COMMAND statediagram_test)
set_target_properties(statediagram PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(statediagram_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
