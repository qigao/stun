add_library(chart_ui STATIC
    src/chart_frame.cpp
    src/flex_node_plot_widget.cpp
)

# XML remains the authoring asset; the native SDK embeds its bytes at configure
# time and never loads templates from the current working directory.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/chart_frame.ui.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/chart_legend_item.ui.xml")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/chart_frame.ui.xml"
    CHART_FRAME_XML_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/chart_legend_item.ui.xml"
    CHART_LEGEND_ITEM_XML_SOURCE)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/chart_frame_xml.inc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/chart_frame_xml.inc" @ONLY)
target_include_directories(chart_ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(chart_ui
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# The adapter includes <flex.h>, whose public DSL headers depend on the
# generated flex_parser_gen.h exported by the Flex::Compiler target.
target_link_libraries(chart_ui PUBLIC flexUI flex)
target_compile_features(chart_ui PUBLIC cxx_std_17)
set_target_properties(chart_ui PROPERTIES FOLDER "Modules/Charts/UI")

add_executable(chart_ui_test tests/chart_frame_tests.cpp)
target_link_libraries(chart_ui_test PRIVATE chart_ui Salts::TinyTest)
add_test(NAME ChartUiTest COMMAND chart_ui_test)
set_target_properties(chart_ui_test PROPERTIES FOLDER "Modules/Charts/UI/Tests")
