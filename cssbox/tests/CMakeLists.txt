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
    # Core cssbox tests
    test_cssbox_comprehensive
    test_cssbox_advanced
    test_cssbox_rough
    test_cssbox_quadtree
    test_cssbox_tailwind
    test_grid_css_parser
    test_css3_features
    test_animation_transforms
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
    target_link_libraries(${test_name} PRIVATE cssbox Catch2::Catch2WithMain fmtlog)
    target_include_directories(${test_name} PRIVATE ../src)
    set_target_properties(${test_name} PROPERTIES FOLDER cssbox/tests/)
    catch_discover_tests(${test_name})
endforeach()

# Standalone tests (not using Catch2)
add_executable(test_memory_integration test_memory_integration.cpp)
target_link_libraries(test_memory_integration PRIVATE cssbox fmtlog)
target_include_directories(test_memory_integration PRIVATE ../src)
set_target_properties(test_memory_integration PROPERTIES FOLDER cssbox/tests/)
add_test(NAME test_memory_integration COMMAND test_memory_integration)