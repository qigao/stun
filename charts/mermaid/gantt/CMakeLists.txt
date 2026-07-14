cmake_minimum_required(VERSION 3.10)
project(gantt_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gantt_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/gantt_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/gantt_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/gantt_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/gantt_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/gantt_parser.y" "${LEMPAR}"
    COMMENT "Generating Gantt C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gantt_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/gantt_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/gantt_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/gantt_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/gantt_parser_gen.h"
    COMMENT "Generating Gantt C lexer with re2c"
)

add_library(gantt STATIC
    src/gantt_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/gantt_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/gantt_parser_gen.c"
)

target_include_directories(gantt PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(gantt PUBLIC TurboUtils::Parser)

# Test executable
add_executable(gantt_test tests/test_parser.c)
target_link_libraries(gantt_test gantt TurboUtils::TinyTest)
target_compile_definitions(gantt_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/gantt")

add_test(NAME gantt_test COMMAND gantt_test)
set_target_properties(gantt PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(gantt_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
