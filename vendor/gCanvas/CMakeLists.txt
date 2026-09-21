cmake_minimum_required(VERSION 3.20)

project(gCanvas VERSION 0.8.0 LANGUAGES C CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(PROJECT_IS_TOP_LEVEL)
  set(GCANVAS_STANDALONE ON)
else()
  set(GCANVAS_STANDALONE OFF)
endif()

option(GCANVAS_BUILD_OPENGL "Build the OpenGL canvas backend" ON)
option(GCANVAS_BUILD_VULKAN "Build the Vulkan canvas backend" ON)
option(GCANVAS_BUILD_SVG "Build the optional PlutoSVG resource adapter" ON)
option(GCANVAS_BUILD_WINDOW "Build the optional GLFW presentation helper" ON)
option(GCANVAS_BUILD_TESTS "Build gCanvas contract tests" ${GCANVAS_STANDALONE})
option(GCANVAS_BUILD_GPU_TESTS "Run real-window GPU pixel tests" ${GCANVAS_STANDALONE})
set(_GCANVAS_BUILD_EXAMPLES_DEFAULT OFF)
if(GCANVAS_STANDALONE AND GCANVAS_BUILD_WINDOW)
  set(_GCANVAS_BUILD_EXAMPLES_DEFAULT ON)
endif()
option(GCANVAS_BUILD_EXAMPLES "Build gCanvas example applications"
       ${_GCANVAS_BUILD_EXAMPLES_DEFAULT})
unset(_GCANVAS_BUILD_EXAMPLES_DEFAULT)
option(GCANVAS_ENABLE_VULKAN_VALIDATION
  "Require the Khronos Vulkan validation layer and debug messenger" OFF)

if(GCANVAS_BUILD_WINDOW AND NOT (GCANVAS_BUILD_OPENGL OR GCANVAS_BUILD_VULKAN))
  message(FATAL_ERROR "GCANVAS_BUILD_WINDOW requires at least one renderer backend")
endif()
if(GCANVAS_BUILD_EXAMPLES AND NOT GCANVAS_BUILD_WINDOW)
  message(FATAL_ERROR "GCANVAS_BUILD_EXAMPLES requires GCANVAS_BUILD_WINDOW")
endif()

find_package(ZLIB MODULE REQUIRED)
find_package(Freetype REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)

add_library(gcanvas_core STATIC
  src/color.cpp
  src/context.cpp
  src/font.cpp
  src/image.cpp
  src/path.cpp
  src/path_tessellator.cpp
  src/vec2.cpp)
add_library(gCanvas::Core ALIAS gcanvas_core)

target_compile_features(gcanvas_core PUBLIC cxx_std_17)
target_include_directories(gcanvas_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${STB_INCLUDE_DIRS})
target_link_libraries(gcanvas_core PRIVATE Freetype::Freetype)
set_target_properties(gcanvas_core PROPERTIES
  EXPORT_NAME Core
  OUTPUT_NAME gcanvas_core
  POSITION_INDEPENDENT_CODE ON)

if(GCANVAS_BUILD_SVG)
  find_package(plutosvg CONFIG REQUIRED)
  add_library(gcanvas_svg STATIC src/svg.cpp)
  add_library(gCanvas::Svg ALIAS gcanvas_svg)
  target_compile_features(gcanvas_svg PUBLIC cxx_std_17)
  target_include_directories(gcanvas_svg
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>)
  target_link_libraries(gcanvas_svg
    PUBLIC gCanvas::Core
    PRIVATE plutosvg::plutosvg)
  set_target_properties(gcanvas_svg PROPERTIES
    EXPORT_NAME Svg
    OUTPUT_NAME gcanvas_svg
    POSITION_INDEPENDENT_CODE ON)
endif()

if(GCANVAS_BUILD_OPENGL)
  find_package(glad CONFIG REQUIRED)
  add_library(gcanvas_opengl STATIC
    src/backends/opengl/context_impl_opengl.cpp
    src/backends/opengl/font_impl_opengl.cpp
    src/backends/opengl/image_impl_opengl.cpp
    src/backends/opengl/opengl_shared_info.cpp)
  add_library(gCanvas::OpenGL ALIAS gcanvas_opengl)

  target_compile_features(gcanvas_opengl PUBLIC cxx_std_17)
  target_include_directories(gcanvas_opengl
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/opengl
      ${STB_INCLUDE_DIRS})
  target_link_libraries(gcanvas_opengl
    PUBLIC gCanvas::Core
    PRIVATE Freetype::Freetype glad::glad)
  set_target_properties(gcanvas_opengl PROPERTIES
    EXPORT_NAME OpenGL
    OUTPUT_NAME gcanvas_opengl
    POSITION_INDEPENDENT_CODE ON)
endif()

if(GCANVAS_BUILD_VULKAN)
  find_package(VulkanHeaders CONFIG REQUIRED)
  find_package(Vulkan REQUIRED)
  find_package(VulkanMemoryAllocator CONFIG REQUIRED)

  add_library(gcanvas_vulkan STATIC
    src/backends/vulkan/context_impl_vulkan.cpp
    src/backends/vulkan/font_impl_vulkan.cpp
    src/backends/vulkan/image_impl_vulkan.cpp
    src/backends/vulkan/vertex.cpp
    src/backends/vulkan/vma.cpp
    src/backends/vulkan/vulkan_shared_info.cpp
    src/backends/vulkan/vulkan_utils.cpp)
  add_library(gCanvas::Vulkan ALIAS gcanvas_vulkan)

  target_compile_features(gcanvas_vulkan PUBLIC cxx_std_17)
  target_include_directories(gcanvas_vulkan
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/vulkan
      ${STB_INCLUDE_DIRS})
  target_link_libraries(gcanvas_vulkan
    PUBLIC
      gCanvas::Core
      Vulkan::Headers
    PRIVATE
      Freetype::Freetype
      GPUOpen::VulkanMemoryAllocator
      Vulkan::Vulkan)
  if(GCANVAS_ENABLE_VULKAN_VALIDATION)
    target_compile_definitions(gcanvas_vulkan PRIVATE
      GCANVAS_ENABLE_VULKAN_VALIDATION=1)
  endif()
  set_target_properties(gcanvas_vulkan PROPERTIES
    EXPORT_NAME Vulkan
    OUTPUT_NAME gcanvas_vulkan
    POSITION_INDEPENDENT_CODE ON)
endif()

foreach(_gcanvas_renderer_target IN ITEMS gcanvas_opengl gcanvas_vulkan)
  if(TARGET ${_gcanvas_renderer_target})
    get_target_property(_gcanvas_renderer_links ${_gcanvas_renderer_target} LINK_LIBRARIES)
    if("${_gcanvas_renderer_links}" MATCHES "(^|;)glfw($|;)|glfw3")
      message(FATAL_ERROR "${_gcanvas_renderer_target} must not link GLFW")
    endif()
  endif()
endforeach()

if(GCANVAS_BUILD_WINDOW)
  find_package(glfw3 CONFIG REQUIRED)
  add_library(gcanvas_window STATIC
    src/event.cpp
    src/window_impl.cpp)
  add_library(gCanvas::Window ALIAS gcanvas_window)

  target_compile_features(gcanvas_window PUBLIC cxx_std_17)
  target_include_directories(gcanvas_window
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${STB_INCLUDE_DIRS})
  target_link_libraries(gcanvas_window
    PUBLIC gCanvas::Core
    PRIVATE glfw)
  if(WIN32)
    target_link_libraries(gcanvas_window PRIVATE user32)
  endif()
  if(GCANVAS_BUILD_OPENGL)
    target_link_libraries(gcanvas_window PUBLIC gCanvas::OpenGL)
    target_compile_definitions(gcanvas_window PRIVATE GCANVAS_HAS_OPENGL=1)
  endif()
  if(GCANVAS_BUILD_VULKAN)
    target_link_libraries(gcanvas_window PUBLIC gCanvas::Vulkan)
    target_compile_definitions(gcanvas_window PRIVATE GCANVAS_HAS_VULKAN=1)
  endif()
  set_target_properties(gcanvas_window PROPERTIES
    EXPORT_NAME Window
    OUTPUT_NAME gcanvas_window
    POSITION_INDEPENDENT_CODE ON)
endif()

add_library(gcanvas INTERFACE)
add_library(gCanvas::gCanvas ALIAS gcanvas)
set_target_properties(gcanvas PROPERTIES EXPORT_NAME gCanvas)
target_link_libraries(gcanvas INTERFACE gCanvas::Core)
if(GCANVAS_BUILD_SVG)
  target_link_libraries(gcanvas INTERFACE gCanvas::Svg)
endif()
if(GCANVAS_BUILD_OPENGL)
  target_link_libraries(gcanvas INTERFACE gCanvas::OpenGL)
endif()
if(GCANVAS_BUILD_VULKAN)
  target_link_libraries(gcanvas INTERFACE gCanvas::Vulkan)
endif()
if(GCANVAS_BUILD_WINDOW)
  target_link_libraries(gcanvas INTERFACE gCanvas::Window)
endif()

if(GCANVAS_BUILD_TESTS)
  enable_testing()
  add_executable(gcanvas_contract_test tests/contract_test.cpp)
  target_link_libraries(gcanvas_contract_test PRIVATE gCanvas::Core)
  add_test(NAME gcanvas_contract COMMAND gcanvas_contract_test)

  add_executable(gcanvas_utf8_test tests/utf8_test.cpp)
  target_compile_features(gcanvas_utf8_test PRIVATE cxx_std_17)
  add_test(NAME gcanvas_utf8 COMMAND gcanvas_utf8_test)

  if(GCANVAS_BUILD_SVG)
    add_executable(gcanvas_svg_test tests/svg_test.cpp)
    target_link_libraries(gcanvas_svg_test PRIVATE gCanvas::Svg)
    add_test(NAME gcanvas_svg COMMAND gcanvas_svg_test)
  endif()

  if(GCANVAS_BUILD_WINDOW)
    add_executable(gcanvas_window_listener_test tests/window_listener_test.cpp)
    target_include_directories(
      gcanvas_window_listener_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(gcanvas_window_listener_test PRIVATE gCanvas::Window)
    add_test(NAME gcanvas_window_listener COMMAND gcanvas_window_listener_test)

    add_executable(gcanvas_backend_link_test tests/backend_link_test.cpp)
    target_link_libraries(gcanvas_backend_link_test PRIVATE gCanvas::Window)
    if(GCANVAS_BUILD_OPENGL)
      target_compile_definitions(gcanvas_backend_link_test PRIVATE GCANVAS_TEST_OPENGL=1)
    endif()
    if(GCANVAS_BUILD_VULKAN)
      target_compile_definitions(gcanvas_backend_link_test PRIVATE GCANVAS_TEST_VULKAN=1)
    endif()
    add_test(NAME gcanvas_backend_link COMMAND gcanvas_backend_link_test)

    if(GCANVAS_BUILD_GPU_TESTS)
      add_executable(gcanvas_gpu_test tests/gpu_smoke_test.cpp)
      target_include_directories(
        gcanvas_gpu_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
      target_link_libraries(gcanvas_gpu_test PRIVATE gCanvas::Window)
      if(GCANVAS_BUILD_OPENGL)
        add_test(NAME gcanvas_gpu_opengl
          COMMAND gcanvas_gpu_test opengl)
        set_tests_properties(gcanvas_gpu_opengl PROPERTIES LABELS "gpu;opengl")
      endif()
      if(GCANVAS_BUILD_VULKAN)
        add_test(NAME gcanvas_gpu_vulkan
          COMMAND gcanvas_gpu_test vulkan)
        set_tests_properties(gcanvas_gpu_vulkan PROPERTIES LABELS "gpu;vulkan")
        if(GCANVAS_ENABLE_VULKAN_VALIDATION)
          set_tests_properties(gcanvas_gpu_vulkan PROPERTIES
            FAIL_REGULAR_EXPRESSION "VULKAN VALIDATION \\[(ERROR|WARNING)\\]")
        endif()
      endif()
    endif()
  endif()
endif()

if(GCANVAS_BUILD_EXAMPLES)
  add_executable(gcanvas_showcase examples/showcase.cpp)
  target_link_libraries(gcanvas_showcase PRIVATE gCanvas::Window)
  if(GCANVAS_BUILD_TESTS)
    if(GCANVAS_BUILD_OPENGL)
      add_test(NAME gcanvas_showcase_opengl COMMAND gcanvas_showcase opengl --smoke)
      set_tests_properties(gcanvas_showcase_opengl PROPERTIES LABELS "app;gpu;opengl")
    endif()
    if(GCANVAS_BUILD_VULKAN)
      add_test(NAME gcanvas_showcase_vulkan COMMAND gcanvas_showcase vulkan --smoke)
      set_tests_properties(gcanvas_showcase_vulkan PROPERTIES LABELS "app;gpu;vulkan")
    endif()
  endif()
endif()

set(_gcanvas_install_targets gcanvas_core gcanvas)
if(GCANVAS_BUILD_SVG)
  list(APPEND _gcanvas_install_targets gcanvas_svg)
endif()
if(GCANVAS_BUILD_OPENGL)
  list(APPEND _gcanvas_install_targets gcanvas_opengl)
endif()
if(GCANVAS_BUILD_VULKAN)
  list(APPEND _gcanvas_install_targets gcanvas_vulkan)
endif()
if(GCANVAS_BUILD_WINDOW)
  list(APPEND _gcanvas_install_targets gcanvas_window)
endif()

install(TARGETS ${_gcanvas_install_targets}
  EXPORT gCanvasTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT gCanvasTargets
  FILE gCanvasTargets.cmake
  NAMESPACE gCanvas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gCanvas)

set(GCANVAS_CONFIG_FIND_GLFW "")
if(GCANVAS_BUILD_WINDOW)
  set(GCANVAS_CONFIG_FIND_GLFW "find_dependency(glfw3 CONFIG)")
endif()
set(GCANVAS_CONFIG_FIND_GLAD "")
if(GCANVAS_BUILD_OPENGL)
  set(GCANVAS_CONFIG_FIND_GLAD "find_dependency(glad CONFIG)")
endif()
set(GCANVAS_CONFIG_FIND_VULKAN "")
if(GCANVAS_BUILD_VULKAN)
  set(GCANVAS_CONFIG_FIND_VULKAN
    "find_dependency(VulkanHeaders CONFIG)\nfind_dependency(Vulkan)\nfind_dependency(VulkanMemoryAllocator CONFIG)")
endif()
set(GCANVAS_CONFIG_FIND_PLUTOSVG "")
if(GCANVAS_BUILD_SVG)
  set(GCANVAS_CONFIG_FIND_PLUTOSVG "find_dependency(plutosvg CONFIG)")
endif()
configure_package_config_file(
  cmake/gCanvasConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/gCanvasConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gCanvas)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/gCanvasConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gCanvasConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/gCanvasConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gCanvas)
