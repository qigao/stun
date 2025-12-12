cmake_minimum_required(VERSION 3.15)
project(FlexChart VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FLEXCHART_BUILD_EXAMPLES "Build FlexChart examples" ON)

set(FLEXCHART_SOURCES
    src/flexchart.cpp
    src/runtime.cpp
    src/render_line.cpp
    src/render_bar.cpp
    src/render_pie.cpp
    src/render_scatter.cpp
    src/render_radar.cpp
    src/render_gauge.cpp
    src/render_funnel.cpp
    src/render_candlestick.cpp
    src/render_heatmap.cpp
    src/render_treemap.cpp
    src/render_boxplot.cpp
    src/render_waterfall.cpp
    src/render_sunburst.cpp
    src/render_polar.cpp
    src/render_ring.cpp
    src/render_parallel.cpp
    src/render_sankey.cpp
    src/render_graph.cpp
    src/render_calendar.cpp
    src/render_themeriver.cpp
    src/render_pictorialbar.cpp
    src/render_liquidfill.cpp
    src/render_tree.cpp
    src/render_bullet.cpp
    src/render_nightingale.cpp
    src/render_histogram.cpp
    src/render_step.cpp
    src/render_barstack.cpp
    src/render_areastack.cpp
    src/render_bubble.cpp
    src/render_lollipop.cpp
    src/render_dumbbell.cpp
    src/render_rangebar.cpp
    src/render_gantt.cpp
    src/render_waffle.cpp
    src/render_radialbar.cpp
    src/render_pyramid.cpp
    src/render_violin.cpp
    src/render_errorbar.cpp
    src/render_slope.cpp
    src/render_dot.cpp
    src/render_map_china.cpp
)

add_library(flexchart STATIC ${FLEXCHART_SOURCES})

target_include_directories(flexchart
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(qjs CONFIG REQUIRED)

target_link_libraries(flexchart
    PUBLIC cssbox nanovg
    PRIVATE nlohmann_json::nlohmann_json qjs fmtlog::fmtlog
)

add_library(FlexChart::FlexChart ALIAS flexchart)

set_target_properties(flexchart PROPERTIES FOLDER flexchart/)

if(FLEXCHART_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
