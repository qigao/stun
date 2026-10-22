cmake_minimum_required(VERSION 3.10)
project(journey_parser C)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/journey_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/journey_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/journey_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/journey_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/journey_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/journey_parser.y" "${LEMPAR}"
    COMMENT "Generating Journey C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/journey_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/journey_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/journey_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/journey_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/journey_parser_gen.h"
    COMMENT "Generating Journey C lexer with re2c"
)

add_library(journey STATIC
    src/journey_parser_wrapper.c
    src/journey_ast.c
    "${CMAKE_CURRENT_BINARY_DIR}/journey_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/journey_parser_gen.c"
)

target_include_directories(journey PUBLIC
    include
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(journey PUBLIC Salts::JsonParser Flex::Expression)

# Test executable
add_executable(journey_test tests/test_parser.c)
target_link_libraries(journey_test journey Salts::TinyTest)
set_target_properties(journey_test PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(journey_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/journey")

add_test(NAME journey_test COMMAND journey_test)
set_target_properties(journey PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(journey_test PROPERTIES FOLDER "Modules/Mermaid/Tests")
