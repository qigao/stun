cmake_minimum_required(VERSION 3.10)
project(mindmap_parser LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)

# Generate Parser
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mindmap_parser_gen.c" "${CMAKE_CURRENT_BINARY_DIR}/mindmap_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/mindmap_parser.y" "${CMAKE_CURRENT_BINARY_DIR}/mindmap_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}" -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/mindmap_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/mindmap_parser.y" "${LEMPAR}"
    COMMENT "Generating Mindmap C parser with lemon"
)

# Generate Lexer
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mindmap_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/mindmap_lexer_gen.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/mindmap_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/mindmap_lexer.re" "${CMAKE_CURRENT_BINARY_DIR}/mindmap_parser_gen.h"
    COMMENT "Generating Mindmap C lexer with re2c"
)

add_library(mindmap STATIC
    src/mindmap_parser_wrapper.c
    "${CMAKE_CURRENT_BINARY_DIR}/mindmap_lexer_gen.c"
    "${CMAKE_CURRENT_BINARY_DIR}/mindmap_parser_gen.c"
)

target_include_directories(mindmap PUBLIC 
    include 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(mindmap PUBLIC TurboParser::Parser flex_runtime)

# Test executable
add_executable(mindmap_test tests/test_parser.c)
target_link_libraries(mindmap_test mindmap Salts::TinyTest)
set_target_properties(mindmap_test PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(mindmap_test PRIVATE GOLDEN_DIR="${MERMAID_GOLDEN_DIR}/mindmap")

add_test(NAME mindmap_test COMMAND mindmap_test)
set_target_properties(mindmap PROPERTIES FOLDER "Modules/Mermaid")
set_target_properties(mindmap_test PROPERTIES FOLDER "Modules/Mermaid/Tests")

# The existing C parser remains independent of native C++ graph placement.
# Stun::GraphTidyTree is a separate algorithm target; this adapter owns only
# Mindmap AST -> tree geometry conversion, not rendering or parser state.
add_library(stun_mermaid_mindmap_layout STATIC src/mindmap_layout.cpp)
add_library(Stun::MermaidMindmapLayout ALIAS stun_mermaid_mindmap_layout)
target_include_directories(stun_mermaid_mindmap_layout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(stun_mermaid_mindmap_layout PUBLIC Stun::GraphTidyTree)
target_compile_features(stun_mermaid_mindmap_layout PUBLIC cxx_std_17)
set_target_properties(stun_mermaid_mindmap_layout PROPERTIES
    FOLDER "Modules/Mermaid/MindmapLayout")
