add_library(chart_ui STATIC
    src/chart_frame.cpp
    src/flex_node_plot_widget.cpp
)

target_include_directories(chart_ui
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(chart_ui PUBLIC flexUI)
target_compile_features(chart_ui PUBLIC cxx_std_17)
set_target_properties(chart_ui PROPERTIES FOLDER "Modules/Charts/UI")

add_executable(chart_ui_test tests/chart_frame_tests.cpp)
target_link_libraries(chart_ui_test PRIVATE chart_ui Salts::TinyTest)
add_test(NAME ChartUiTest COMMAND chart_ui_test)
set_target_properties(chart_ui_test PROPERTIES FOLDER "Modules/Charts/UI/Tests")
