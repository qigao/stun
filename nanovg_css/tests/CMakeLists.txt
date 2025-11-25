# ============================================================================
# Tests
# ============================================================================
# Find OpenGL
find_package(OpenGL REQUIRED)
# Find Catch2
find_package(Catch2 3 REQUIRED)

# Comprehensive Catch2 test suite
add_executable(test_nanovg_css_comprehensive test_nanovg_css_comprehensive.cpp)
target_link_libraries(test_nanovg_css_comprehensive
    PRIVATE
        nanovg::css 
        Catch2::Catch2WithMain
)
set_target_properties(test_nanovg_css_comprehensive PROPERTIES FOLDER nanovg_css/tests/)

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(test_nanovg_css_comprehensive)

# Advanced features test suite
add_executable(test_nanovg_css_advanced test_nanovg_css_advanced.cpp)
target_link_libraries(test_nanovg_css_advanced
    PRIVATE
        nanovg::css
        Catch2::Catch2WithMain
)
set_target_properties(test_nanovg_css_advanced PROPERTIES FOLDER nanovg_css/tests/)
catch_discover_tests(test_nanovg_css_advanced)

# Flexbox layout comprehensive test suite
add_executable(test_nanovg_css_flexbox test_nanovg_css_flexbox.cpp)
target_link_libraries(test_nanovg_css_flexbox
    PRIVATE
        nanovg::css
        Catch2::Catch2WithMain
)
set_target_properties(test_nanovg_css_flexbox PROPERTIES FOLDER nanovg_css/tests/)
catch_discover_tests(test_nanovg_css_flexbox)

# Flexbox extra test suite (Phase 4 regression tests)
add_executable(test_nanovg_css_flexbox_extra test_nanovg_css_flexbox_extra.cpp)
target_link_libraries(test_nanovg_css_flexbox_extra
    PRIVATE
        nanovg::css
        Catch2::Catch2WithMain
)
set_target_properties(test_nanovg_css_flexbox_extra PROPERTIES FOLDER nanovg_css/tests/)
catch_discover_tests(test_nanovg_css_flexbox_extra)

# Grid layout comprehensive test suite
add_executable(test_nanovg_css_grid test_nanovg_css_grid.cpp)
target_link_libraries(test_nanovg_css_grid
    PRIVATE
        nanovg::css
        Catch2::Catch2WithMain
)
set_target_properties(test_nanovg_css_grid PROPERTIES FOLDER nanovg_css/tests/)
catch_discover_tests(test_nanovg_css_grid)

# Computed CSS comprehensive test suite
add_executable(test_nanovg_css_computed test_nanovg_css_computed.cpp)
target_link_libraries(test_nanovg_css_computed
    PRIVATE
        nanovg::css 
        Catch2::Catch2WithMain
)
set_target_properties(test_nanovg_css_computed PROPERTIES FOLDER nanovg_css/tests/)
catch_discover_tests(test_nanovg_css_computed)
