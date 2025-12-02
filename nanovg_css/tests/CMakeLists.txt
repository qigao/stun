# ============================================================================
# Tests
# ============================================================================
find_package(OpenGL REQUIRED)
find_package(Catch2 3 REQUIRED)

include(CTest)
include(Catch)

# ============================================================================
# All test suites (unified configuration)
# ============================================================================
set(ALL_TESTS
    # Core nanovg_css tests
    test_nanovg_css_comprehensive
    test_nanovg_css_advanced
    test_nanovg_css_flexbox
    test_nanovg_css_flexbox_extra
    test_nanovg_css_grid
    test_nanovg_css_computed
    test_nanovg_rough
    test_nanovg_css_quadtree
    test_nanovg_css_tailwind
    # SVG feature tests
    test_svg_path_parser
    test_svg_polygon_polyline
    test_svg_markers
    test_marker_css
    test_path_sampler
    test_clipping
    test_patterns
    test_text_on_path
)

foreach(test_name ${ALL_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE nanovg_css Catch2::Catch2WithMain fmtlog)
    target_include_directories(${test_name} PRIVATE ../src)
    set_target_properties(${test_name} PROPERTIES FOLDER nanovg_css/tests/)
    catch_discover_tests(${test_name})
endforeach()