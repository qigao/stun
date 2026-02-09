cmake_minimum_required(VERSION 3.10)
project(er_parser C)

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
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/er_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/er_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/er_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/er_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/er_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/er_parser.y" "${LEMPAR}"
    COMMENT "Generating ER C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/er_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/er_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/er_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/er_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/er_parser_gen.h"
    COMMENT "Generating ER C lexer with re2c"
)

add_library(er STATIC
    src/er_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/er_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/er_parser_gen.c"
)

target_include_directories(er PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(er PUBLIC TurboNet::Parser)

# Test executable
add_executable(er_test tests/test_parser.c)
target_link_libraries(er_test er TurboNet::TinyTest TurboNet::Parser)
target_compile_definitions(er_test PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/charts/mermaid/golden/er")

add_test(NAME er_test COMMAND er_test)
set_target_properties(er PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(er_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
