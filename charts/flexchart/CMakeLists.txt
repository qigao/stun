# Flex Chart Module
cmake_minimum_required(VERSION 3.16)

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

# Generate parser
set(CHART_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/compiler/chart_grammar.y)
set(CHART_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/chart_parser_gen.c)
set(CHART_PARSER_GEN ${CMAKE_CURRENT_BINARY_DIR}/chart_parser_gen.cpp)
set(CHART_PARSER_H ${CMAKE_CURRENT_BINARY_DIR}/chart_parser_gen.h)

add_custom_command(
    OUTPUT ${CHART_PARSER_GEN} ${CHART_PARSER_H}
    COMMAND ${CMAKE_COMMAND} -E copy ${CHART_PARSER_Y} ${CMAKE_CURRENT_BINARY_DIR}/chart_parser_gen.y
    COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR} ${CMAKE_CURRENT_BINARY_DIR}/chart_parser_gen.y
    COMMAND ${CMAKE_COMMAND} -E rename ${CHART_PARSER_GEN_C} ${CHART_PARSER_GEN}
    DEPENDS ${CHART_PARSER_Y}
    COMMENT "Generating Chart DSL parser with lemon"
    VERBATIM
)

# Generate lexer
set(CHART_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/compiler/chart_lexer.re)
set(CHART_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/chart_lexer_gen.cpp)

add_custom_command(
    OUTPUT ${CHART_LEXER_GEN}
    COMMAND ${RE2C_EXECUTABLE} -o ${CHART_LEXER_GEN} ${CHART_LEXER_RE}
    DEPENDS ${CHART_LEXER_RE} ${CHART_PARSER_H}
    COMMENT "Generating Chart DSL lexer with re2c"
    VERBATIM
)

# ============================================================================
# FlexChart Library
# ============================================================================

add_library(flexchart STATIC
    ${CHART_LEXER_GEN}
    ${CHART_PARSER_GEN}
    compiler/chart_parser.cpp
    src/flex_chart.cpp
    src/chart_component.cpp
    src/chart_component_helpers.cpp
    src/bar_mark_renderer.cpp
    src/pie_mark_renderer.cpp
    src/line_mark_renderer.cpp
    src/area_mark_renderer.cpp
    src/point_mark_renderer.cpp
    src/rect_mark_renderer.cpp
    src/boxplot_mark_renderer.cpp
    src/radar_mark_renderer.cpp
    src/text_mark_renderer.cpp
    src/rule_mark_renderer.cpp
    src/arc_mark_renderer.cpp
    src/tick_mark_renderer.cpp
    src/errorbar_mark_renderer.cpp
)

target_include_directories(flexchart
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        compiler
)

target_link_libraries(flexchart PUBLIC flex TurboNet::Utils)

target_compile_features(flexchart PUBLIC cxx_std_17)

set_target_properties(flexchart PROPERTIES FOLDER "Modules/FlexChart")

# ============================================================================
# Examples
# ============================================================================

find_package(SDL2 CONFIG QUIET)
if (SDL2_FOUND)
    add_executable(basic_chart_example examples/basic_chart_example.cpp)
    target_link_libraries(basic_chart_example PRIVATE flexchart flex SDL2::SDL2 SDL2::SDL2main)
    set_target_properties(basic_chart_example PROPERTIES FOLDER "Modules/FlexChart/Examples")

    # Copy chart examples to binary dir
    file(GLOB CHART_FILES examples/*.chart)
    foreach(CHART_FILE ${CHART_FILES})
        get_filename_component(CHART_NAME ${CHART_FILE} NAME)
        configure_file(${CHART_FILE} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CHART_NAME} COPYONLY)
    endforeach()
endif()

# ============================================================================
# Tests
# ============================================================================

add_subdirectory(tests)
