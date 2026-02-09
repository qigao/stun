cmake_minimum_required(VERSION 3.10)
project(kanban_parser C)

set(CMAKE_C_STANDARD 11)

if(NOT RE2C_EXECUTABLE OR NOT LEMON_EXECUTABLE)
    find_program(RE2C_EXECUTABLE re2c REQUIRED)
    find_program(LEMON_EXECUTABLE lemon REQUIRED)
endif()

if(NOT LEMPAR)
    set(LEMPAR ${CMAKE_SOURCE_DIR}/vendor/lemon/lempar.c)
endif()

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kanban_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/kanban_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/kanban_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/kanban_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/kanban_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/kanban_parser.y" "${LEMPAR}"
    COMMENT "Generating Kanban C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kanban_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/kanban_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/kanban_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/kanban_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/kanban_parser_gen.h"
    COMMENT "Generating Kanban C lexer with re2c"
)

add_library(kanban STATIC
    src/kanban_parser_wrapper.c
    src/kanban_ast.c
    "${CMAKE_CURRENT_BINARY_DIR}/kanban_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/kanban_parser_gen.c"
)

target_include_directories(kanban PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(kanban PUBLIC TurboNet::Parser)

# Test executable
add_executable(kanban_test tests/test_parser.c)
target_link_libraries(kanban_test kanban TurboNet::TinyTest)
target_compile_definitions(kanban_test PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/charts/mermaid/golden/kanban")

add_test(NAME kanban_test COMMAND kanban_test)
set_target_properties(kanban PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(kanban_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
