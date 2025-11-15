# ============================================================================
# Tests
# ============================================================================
 
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

 