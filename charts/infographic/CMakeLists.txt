# FlexInfographic - Infographic DSL with re2c + lemon parser
cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Parser Generation (re2c + lemon)
# ============================================================================
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
# Infographic Core Library
# ============================================================================

add_library(infographic_core STATIC
    # Generated parser/lexer
    ${INFO_LEXER_GEN}
    ${INFO_PARSER_GEN}
    compiler/infographic_parser.cpp
    
    # Legacy IR & Parser (for backward compatibility)
    src/ir/unified_infographic.cpp 
    src/parser/unified_parser.cpp
    
    # Core API
    src/flexinfographic_new.cpp 
    src/flexui_infographic.cpp
    src/infographic_component.cpp
    
    # Render Helpers
    src/render_helpers.cpp

    # Layout Engine
    src/layout/layout_engine.cpp
    
    # Renderer (Strategy Pattern)
    src/renderer/template_renderer.cpp
    src/renderer/list_renderer.cpp
    src/renderer/sequence_renderer.cpp
    src/renderer/compare_renderer.cpp
    src/renderer/chart_renderer.cpp
    src/renderer/hierarchy_renderer.cpp
)

target_include_directories(infographic_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/infographic>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/compiler
)

target_link_libraries(infographic_core PUBLIC flex chart_ui cola)

target_compile_features(infographic_core PUBLIC cxx_std_17)

set_target_properties(infographic_core PROPERTIES FOLDER "Modules/FlexInfographic/Core")

# ============================================================================
# Layout Modules (by layout type)
# ============================================================================

add_subdirectory(grid)
add_subdirectory(timeline)
add_subdirectory(funnel)
add_subdirectory(pie)
add_subdirectory(bar)
add_subdirectory(swot)
add_subdirectory(tree)
add_subdirectory(zigzag)
add_subdirectory(circular)
add_subdirectory(roadmap)

# ============================================================================
# Umbrella Library (Backwards Compatible)
# ============================================================================

add_library(flexinfographic STATIC
    src/layout_renderer_factory.cpp
)

target_link_libraries(flexinfographic
    PUBLIC
        infographic_core
        infographic_layout_grid
        infographic_layout_timeline
        infographic_layout_funnel
        infographic_layout_pie
        infographic_layout_bar
        infographic_layout_swot
        infographic_layout_tree
        infographic_layout_zigzag
        infographic_layout_circular
        infographic_layout_roadmap
)

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
add_test(NAME FlexInfographicNewTemplatesTest COMMAND flexinfographic_new_templates_test)
set_target_properties(flexinfographic_new_templates_test PROPERTIES FOLDER "Modules/FlexInfographic/Tests")

# The former SDL software-rendered examples depended on the retired ThorVG
# engine. The renderer-independent examples and end-to-end tests stay enabled.

# ============================================================================
# Tests
# ============================================================================

add_executable(flexinfographic_tests tests/unified_design_test.cpp)
target_link_libraries(flexinfographic_tests flexinfographic)
add_test(NAME FlexInfographicTests COMMAND flexinfographic_tests)
set_target_properties(flexinfographic_tests PROPERTIES FOLDER "Modules/FlexInfographic/Tests")

add_executable(flexinfographic_dsl_test tests/test_dsl_parser.cpp)
target_link_libraries(flexinfographic_dsl_test flexinfographic)
add_test(NAME FlexInfographicDslTest COMMAND flexinfographic_dsl_test)
set_target_properties(flexinfographic_dsl_test PROPERTIES FOLDER "Modules/FlexInfographic/Tests")


add_executable(flexinfographic_e2e_test tests/test_e2e_pipeline.cpp)
target_link_libraries(flexinfographic_e2e_test flexinfographic)
add_test(NAME FlexInfographicE2ETest COMMAND flexinfographic_e2e_test)
set_target_properties(flexinfographic_e2e_test PROPERTIES FOLDER "Modules/FlexInfographic/Tests")

add_executable(flexinfographic_unified_parser_test tests/test_unified_parser.cpp)
target_link_libraries(flexinfographic_unified_parser_test PRIVATE
    flexinfographic Salts::TinyTest)
add_test(NAME FlexInfographicUnifiedParserTest COMMAND flexinfographic_unified_parser_test)
set_target_properties(flexinfographic_unified_parser_test PROPERTIES
    FOLDER "Modules/FlexInfographic/Tests")
