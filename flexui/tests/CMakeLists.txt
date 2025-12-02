# FlexUI Unit Tests

# Find Catch2
find_package(Catch2 3 REQUIRED)

# Test executable
add_executable(test_flexui
  test_flexui_core.cpp
  test_spatial_index_simple.cpp
  test_css_styling.cpp
  test_flexui_tailwind_features.cpp
  test_svg_rendering.cpp
)

target_link_libraries(
  test_flexui
  PRIVATE flexui_core Catch2::Catch2WithMain)

target_include_directories(
  test_flexui
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../nanovg_css/include
)

set_target_properties(
  test_flexui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
                         FOLDER "flexui/tests")

# Register with CTest
include(CTest)
include(Catch)
catch_discover_tests(test_flexui)

# Add to test target
add_test(NAME FlexUI::Tests COMMAND test_flexui)
