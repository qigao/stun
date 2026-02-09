# Flowchart C Module - Standalone Parser
cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Parser Generation (re2c + lemon)
# ============================================================================

if(NOT RE2C_EXECUTABLE OR NOT LEMON_EXECUTABLE)
    find_program(RE2C_EXECUTABLE re2c)
    find_program(LEMON_EXECUTABLE lemon)
endif()

if(NOT LEMPAR)
    set(LEMPAR ${CMAKE_SOURCE_DIR}/vendor/lemon/lempar.c)
endif()

set(FC_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/flowchart_parser.y)
set(FC_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.c)
set(FC_PARSER_GEN_H ${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.h)

add_custom_command(
    OUTPUT ${FC_PARSER_GEN_C} ${FC_PARSER_GEN_H}
    COMMAND ${CMAKE_COMMAND} -E copy ${FC_PARSER_Y} ${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.y
    COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR} ${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.y
    DEPENDS ${FC_PARSER_Y} ${LEMPAR}
    COMMENT "Generating Flowchart C parser with lemon"
    VERBATIM
)

set(FC_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/flowchart_lexer.re)
set(FC_LEXER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/flowchart_lexer_gen.c)

add_custom_command(
    OUTPUT ${FC_LEXER_GEN_C}
    COMMAND ${RE2C_EXECUTABLE} -o ${FC_LEXER_GEN_C} ${FC_LEXER_RE}
    DEPENDS ${FC_LEXER_RE} ${FC_PARSER_GEN_H}
    COMMENT "Generating Flowchart C lexer with re2c"
    VERBATIM
)

# ============================================================================
# Flowchart Library (mermaid_flowchart)
# ============================================================================

add_library(mermaid_flowchart STATIC
    ${FC_LEXER_GEN_C}
    ${FC_PARSER_GEN_C}
    src/flowchart_parser_wrapper.c
    src/flowchart_renderer.cpp
    src/flex_flowchart.cpp
)

target_include_directories(mermaid_flowchart
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        src
)
 
target_link_libraries(mermaid_flowchart 
    PUBLIC TurboNet::Parser flex_runtime TurboNet::Mustache
)
# Windows: disable min/max macros that conflict with std::min/std::max
if(WIN32)
    target_compile_definitions(mermaid_flowchart PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ============================================================================
# Tests
# ============================================================================

# 原有的 Parser 测试
add_executable(flowchart_test tests/test_parser.c)
target_link_libraries(flowchart_test PRIVATE mermaid_flowchart TurboNet::TinyTest TurboNet::Parser)
target_compile_definitions(flowchart_test PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/charts/mermaid/golden/flowchart")
set_target_properties(flowchart_test PROPERTIES FOLDER "Modules/Mermaid/Tests")

# 新的 Renderer 测试
add_executable(flowchart_renderer_test tests/test_renderer.cpp)
target_link_libraries(flowchart_renderer_test PRIVATE mermaid_flowchart)
set_target_properties(flowchart_renderer_test PROPERTIES FOLDER "Modules/Mermaid/Tests")

# 新的 Flex 集成测试
add_executable(flowchart_flex_test tests/test_flex.cpp)
target_link_libraries(flowchart_flex_test PRIVATE mermaid_flowchart flex_runtime)
set_target_properties(flowchart_flex_test PROPERTIES FOLDER "Modules/Mermaid/Tests")

# 终极 GUI 演示
find_package(SDL2 REQUIRED)
add_executable(flowchart_gui_demo tests/gui_demo.cpp)
target_link_libraries(flowchart_gui_demo PRIVATE 
    mermaid_flowchart 
    flex_runtime 
    flex
    SDL2::SDL2
    thorvg
)
set_target_properties(flowchart_gui_demo PROPERTIES FOLDER "Modules/Mermaid/Tests")

set_target_properties(mermaid_flowchart PROPERTIES FOLDER "Modules/Mermaid")
