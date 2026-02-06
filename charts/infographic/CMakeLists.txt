# FlexInfographic - Infographic DSL with re2c + lemon parser
cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Parser Generation (re2c + lemon)
# ============================================================================

# Use re2c and lemon from parent flex project
if(NOT RE2C_EXECUTABLE OR NOT LEMON_EXECUTABLE)
    find_program(RE2C_EXECUTABLE re2c)
    find_program(LEMON_EXECUTABLE lemon)
endif()

if(NOT LEMPAR)
    set(LEMPAR ${CMAKE_SOURCE_DIR}/vendor/lemon/lempar.c)
endif()

# Generate parser with lemon
set(INFO_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/compiler/infographic_grammar.y)
set(INFO_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/infographic_parser_gen.c)
set(INFO_PARSER_GEN ${CMAKE_CURRENT_BINARY_DIR}/infographic_parser_gen.cpp)
set(INFO_PARSER_H ${CMAKE_CURRENT_BINARY_DIR}/infographic_parser_gen.h)

add_custom_command(
    OUTPUT ${INFO_PARSER_GEN} ${INFO_PARSER_H}
    COMMAND ${CMAKE_COMMAND} -E copy ${INFO_PARSER_Y} ${CMAKE_CURRENT_BINARY_DIR}/infographic_parser_gen.y
    COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR} ${CMAKE_CURRENT_BINARY_DIR}/infographic_parser_gen.y
    COMMAND ${CMAKE_COMMAND} -E rename ${INFO_PARSER_GEN_C} ${INFO_PARSER_GEN}
    DEPENDS ${INFO_PARSER_Y}
    COMMENT "Generating Infographic DSL parser with lemon"
    VERBATIM
)

# Generate lexer with re2c
set(INFO_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/compiler/infographic_lexer.re)
set(INFO_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/infographic_lexer_gen.cpp)

add_custom_command(
    OUTPUT ${INFO_LEXER_GEN}
    COMMAND ${RE2C_EXECUTABLE} -o ${INFO_LEXER_GEN} ${INFO_LEXER_RE}
    DEPENDS ${INFO_LEXER_RE} ${INFO_PARSER_H}
    COMMENT "Generating Infographic DSL lexer with re2c"
    VERBATIM
)

# ============================================================================
# FlexInfographic Library
# ============================================================================

add_library(flexinfographic STATIC
    # Generated parser/lexer
    ${INFO_LEXER_GEN}
    ${INFO_PARSER_GEN}
    compiler/infographic_parser.cpp
    
    # Legacy IR & Parser (for backward compatibility)
    src/ir/unified_infographic.cpp 
    src/parser/unified_parser.cpp
    
    # Core API
    src/flexinfographic_new.cpp 
    src/infographic_component.cpp
    
    # Render Helpers
    src/render_helpers.cpp
    src/layout_renderer_factory.cpp
    
    # Layout Renderers (Factory Pattern)
    src/renderers/grid_layout_renderer.cpp
    src/renderers/timeline_layout_renderer.cpp
    src/renderers/funnel_layout_renderer.cpp
    src/renderers/pie_layout_renderer.cpp
    src/renderers/bar_layout_renderer.cpp
    src/renderers/swot_layout_renderer.cpp
    src/renderers/tree_layout_renderer.cpp
    src/renderers/zigzag_layout_renderer.cpp
    src/renderers/circular_layout_renderer.cpp
    src/renderers/roadmap_layout_renderer.cpp
    
    # Layout Engine
    src/layout/layout_engine.cpp
    
    # Renderer (Strategy Pattern)
    src/renderer/template_renderer.cpp
    src/renderer/list_renderer.cpp
    src/renderer/sequence_renderer.cpp
    src/renderer/compare_renderer.cpp
    src/renderer/chart_renderer.cpp
)

target_include_directories(flexinfographic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/infographic>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/compiler
)

target_link_libraries(flexinfographic PUBLIC flex)

target_compile_features(flexinfographic PUBLIC cxx_std_17)

set_target_properties(flexinfographic PROPERTIES FOLDER "Modules/FlexInfographic")

# ============================================================================
# Examples
# ============================================================================

add_executable(flexinfographic_example examples/simple_example.cpp)
target_link_libraries(flexinfographic_example flexinfographic)
set_target_properties(flexinfographic_example PROPERTIES FOLDER "Modules/FlexInfographic/Examples")

add_executable(flexinfographic_new_templates_test examples/new_templates_test.cpp)
target_link_libraries(flexinfographic_new_templates_test flexinfographic)
set_target_properties(flexinfographic_new_templates_test PROPERTIES FOLDER "Modules/FlexInfographic/Tests")

find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
    add_executable(infographic_render_example examples/infographic_render_example.cpp)
    target_link_libraries(infographic_render_example PRIVATE flexinfographic flex SDL2::SDL2 SDL2::SDL2main)
    set_target_properties(infographic_render_example PROPERTIES FOLDER "Modules/FlexInfographic/Examples")

    add_executable(infographic_dsl_render examples/dsl_render_example.cpp)
    target_link_libraries(infographic_dsl_render PRIVATE flexinfographic flex SDL2::SDL2 SDL2::SDL2main)
    set_target_properties(infographic_dsl_render PROPERTIES FOLDER "Modules/FlexInfographic/Examples")
endif()

# ============================================================================
# Tests
# ============================================================================

add_executable(flexinfographic_tests tests/unified_design_test.cpp)
target_link_libraries(flexinfographic_tests flexinfographic)
set_target_properties(flexinfographic_tests PROPERTIES FOLDER "Modules/FlexInfographic/Tests")

add_executable(flexinfographic_dsl_test tests/test_dsl_parser.cpp)
target_link_libraries(flexinfographic_dsl_test flexinfographic)
set_target_properties(flexinfographic_dsl_test PROPERTIES FOLDER "Modules/FlexInfographic/Tests")


add_executable(flexinfographic_e2e_test tests/test_e2e_pipeline.cpp)
target_link_libraries(flexinfographic_e2e_test flexinfographic)
set_target_properties(flexinfographic_e2e_test PROPERTIES FOLDER "Modules/FlexInfographic/Tests")
