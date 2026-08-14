find_package(Vulkan REQUIRED)

add_library(flex_backend_vulkan STATIC renderer.cpp)
target_link_libraries(
  flex_backend_vulkan
  PUBLIC flex_render Vulkan::Vulkan
  PRIVATE flex_render_engine_gcanvas gCanvas::Vulkan)
target_include_directories(
  flex_backend_vulkan
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(flex_backend_vulkan PUBLIC cxx_std_17)
set_target_properties(
  flex_backend_vulkan
  PROPERTIES FOLDER "flex/backends" POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTING)
  add_executable(test_flex_backend_vulkan tests/test_vulkan_backend.cpp)
  target_link_libraries(
    test_flex_backend_vulkan
    PRIVATE flex_backend_vulkan TurboUtils::TinyTest)
  set_target_properties(
    test_flex_backend_vulkan PROPERTIES FOLDER "flex/backends/tests")
  add_test(NAME test_flex_backend_vulkan COMMAND test_flex_backend_vulkan)

  add_executable(test_flex_backend_vulkan_roundtrip tests/test_vulkan_roundtrip.cpp)
  target_link_libraries(
    test_flex_backend_vulkan_roundtrip
    PRIVATE flex_backend_vulkan TurboUtils::TinyTest)
  set_target_properties(
    test_flex_backend_vulkan_roundtrip PROPERTIES FOLDER "flex/backends/tests")
  add_test(
    NAME test_flex_backend_vulkan_roundtrip
    COMMAND test_flex_backend_vulkan_roundtrip)
endif()
