# Flex Chart Module
cmake_minimum_required(VERSION 3.16)

# ============================================================================
# FlexChart Library (core — no unified parser)
# ============================================================================

add_library(flexchart STATIC
    src/flex_chart.cpp
    src/chart_component.cpp
    src/chart_component_helpers.cpp
    src/chart_parser_dispatch.cpp
    src/flexui_chart.cpp
    src/mark_renderer_registry.cpp
)

target_include_directories(flexchart
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        src
        # Per-mark parser headers (used by flex_chart.cpp dispatcher)
        marks/bar/include
        marks/line/include
        marks/pie/include
        marks/arc/include
        marks/area/include
        marks/point/include
        marks/rect/include
        marks/rule/include
        marks/tick/include
        marks/text/include
        marks/radar/include
        marks/boxplot/include
        marks/errorbar/include
        marks/trail/include
        marks/errorband/include
        marks/geoshape/include
        marks/image/include
        marks/common/include
)

target_link_libraries(flexchart PUBLIC flex chart_ui TurboUtils::Core)

target_compile_features(flexchart PUBLIC cxx_std_17)

set_target_properties(flexchart PROPERTIES FOLDER "Modules/FlexChart")

# ============================================================================
# Per-Mark Modules (each with own parser/lexer)
# ============================================================================

add_subdirectory(marks)

set(FLEXCHART_MARK_TARGETS
    flexchart_bar flexchart_line flexchart_arc flexchart_area
    flexchart_point flexchart_rect flexchart_rule flexchart_tick
    flexchart_text flexchart_pie flexchart_radar flexchart_boxplot
    flexchart_errorbar flexchart_trail flexchart_errorband
    flexchart_geoshape flexchart_image
)

target_link_libraries(flexchart PUBLIC ${FLEXCHART_MARK_TARGETS})

# Convenience target that links all mark modules
add_library(flexchart_all_marks INTERFACE)
target_link_libraries(flexchart_all_marks INTERFACE
    flexchart ${FLEXCHART_MARK_TARGETS})

# ============================================================================
# Examples
# ============================================================================

find_package(SDL2 CONFIG QUIET)
if (SDL2_FOUND)
    add_executable(basic_chart_example examples/basic_chart_example.cpp)
    target_link_libraries(basic_chart_example PRIVATE flexchart_all_marks flex flex_backend_thorvg SDL2::SDL2 SDL2::SDL2main)
    set_target_properties(basic_chart_example PROPERTIES FOLDER "Modules/FlexChart/Examples")

    # Copy chart examples to binary dir
    file(GLOB CHART_FILES examples/*.chart)
 
endif()

# ============================================================================
# Tests
# ============================================================================

add_subdirectory(tests)
