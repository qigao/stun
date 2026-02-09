cmake_minimum_required(VERSION 3.10)
project(classdiagram_parser C)

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
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/classdiagram_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/classdiagram_parser.y" "${LEMPAR}"
    COMMENT "Generating ClassDiagram C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/classdiagram_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/classdiagram_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_parser_gen.h"
    COMMENT "Generating ClassDiagram C lexer with re2c"
)

add_library(classdiagram STATIC
    src/classdiagram_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/classdiagram_parser_gen.c"
)

target_link_libraries(classdiagram PUBLIC TurboNet::Parser)

target_include_directories(classdiagram PUBLIC
    include
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Test executable
add_executable(classdiagram_test tests/test_parser.c)
target_link_libraries(classdiagram_test classdiagram TurboNet::TinyTest TurboNet::Parser)
target_compile_definitions(classdiagram_test PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/charts/mermaid/golden/class")

add_test(NAME classdiagram_test COMMAND classdiagram_test)
set_target_properties(classdiagram PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(classdiagram_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
