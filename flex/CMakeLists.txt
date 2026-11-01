# Flex Engine - Visual Core with DSL Parser
cmake_minimum_required(VERSION 3.16)

project(flex LANGUAGES C CXX)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
find_package(Stb REQUIRED)

set(FLEX_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(FLEX_ENGINE_INCLUDE_DIR "${FLEX_ENGINE_ROOT}/include")
set(FLEX_ENGINE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# The implementation modules own their source lists and dependency boundaries.
# Legacy target names remain concrete targets; namespaced aliases provide the
# stable product-facing API for new consumers.
add_subdirectory(modules/expression)
add_subdirectory(modules/animation)
add_subdirectory(modules/render)
add_subdirectory(modules/runtime)
add_subdirectory(modules/dsl)
add_subdirectory(modules/engine)

# ============================================================================
# Optional Renderer Backend Libraries
# ============================================================================

option(FLEX_BUILD_RENDER_ENGINE_GCANVAS
       "Build the internal gCanvas rendering engine" ON)
if(FLEX_BUILD_RENDER_ENGINE_GCANVAS)
  # Never silently disable a requested backend when its provider is absent.
  add_subdirectory(render/engines/gcanvas)
endif()

set(FLEX_OPENGL_RENDER_ENGINE "gcanvas" CACHE STRING
    "Flex OpenGL drawing engine: gcanvas or nanovg")
set_property(CACHE FLEX_OPENGL_RENDER_ENGINE PROPERTY STRINGS gcanvas nanovg)
if(NOT FLEX_OPENGL_RENDER_ENGINE STREQUAL "gcanvas" AND
   NOT FLEX_OPENGL_RENDER_ENGINE STREQUAL "nanovg")
  message(FATAL_ERROR
    "FLEX_OPENGL_RENDER_ENGINE must be either gcanvas or nanovg")
endif()
option(FLEX_BUILD_RENDER_ENGINE_NANOVG
       "Build the legacy NanoVG engine for migration parity tests" OFF)

option(FLEX_BUILD_BACKEND_OPENGL "Build OpenGL renderer backend" ON)
option(FLEX_BUILD_BACKEND_TUI "Build terminal UI renderer backend" ON)
option(FLEX_BUILD_BACKEND_D2D "Build Direct2D renderer backend" ON)
option(FLEX_BUILD_BACKEND_VULKAN "Build native Vulkan renderer backend" ON)

if(FLEX_BUILD_BACKEND_OPENGL)
  if(FLEX_BUILD_RENDER_ENGINE_NANOVG OR
     FLEX_OPENGL_RENDER_ENGINE STREQUAL "nanovg")
    add_subdirectory(render/engines/nanovg)
  endif()
  add_subdirectory(backends/opengl)
endif()
if(FLEX_BUILD_BACKEND_TUI)
  add_subdirectory(backends/tui)
endif()
if(FLEX_BUILD_BACKEND_D2D)
  add_subdirectory(backends/d2d)
endif()
if(FLEX_BUILD_BACKEND_VULKAN)
  add_subdirectory(backends/vulkan)
endif()

# ============================================================================
# Binary Format Library (optional)
# ============================================================================

option(FLEX_BUILD_BINARY "Build binary format library" ON)
if(FLEX_BUILD_BINARY)
  add_subdirectory(binary)
  message(STATUS "Flex: Binary format library enabled")
endif()

# ============================================================================
# Compiler Tool (optional)
# ============================================================================

option(FLEX_BUILD_TOOLS "Build compiler tools" ON)
if(FLEX_BUILD_TOOLS AND FLEX_BUILD_BINARY)
  add_subdirectory(tools)
  message(STATUS "Flex: Compiler tools enabled")
endif()

# ============================================================================
# Examples & Tests
# ============================================================================

if(NANOGUI_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# ============================================================================
# Install
# ============================================================================

set(FLEX_EXPORT_TARGETS
  flex
  flex_expression
  flex_animation
  flex_render
  flex_compiler
  flex_runtime
  flex_dsl
  flex_core)

# Export the exact native libraries used by FlexUI::Core. Private static
# dependencies are real imported Flex::Vendor* archives, not linker-name
# fallbacks or absolute source-tree paths.
set_target_properties(
  flex PROPERTIES EXPORT_NAME Engine)
set_target_properties(
  flex_expression PROPERTIES EXPORT_NAME Expression)
set_target_properties(
  flex_animation PROPERTIES EXPORT_NAME Animation)
set_target_properties(
  flex_render PROPERTIES EXPORT_NAME Render)
set_target_properties(
  flex_runtime PROPERTIES EXPORT_NAME Runtime)
set_target_properties(
  flex_compiler PROPERTIES EXPORT_NAME Compiler)
set_target_properties(
  flex_core PROPERTIES EXPORT_NAME Core)
set_target_properties(
  flex_dsl PROPERTIES EXPORT_NAME DSL)
set_target_properties(
  mir_static PROPERTIES EXPORT_NAME VendorMIR)
set_target_properties(
  miniblas_linalg PROPERTIES EXPORT_NAME VendorMiniBLAS)

install(
  TARGETS ${FLEX_EXPORT_TARGETS} mir_static miniblas_linalg
  EXPORT FlexTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT FlexWidgetSDK)
install(EXPORT FlexTargets
  FILE FlexTargets.cmake
  NAMESPACE Flex::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Flex
  COMPONENT FlexWidgetSDK)

configure_package_config_file(
  cmake/FlexConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/FlexConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Flex")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/FlexConfigVersion.cmake"
  VERSION "1.0.0"
  COMPATIBILITY SameMajorVersion)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/FlexConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FlexConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Flex"
  COMPONENT FlexWidgetSDK)

install(
  DIRECTORY include/flex
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT FlexWidgetSDK
  PATTERN "bridge/renderer_*.h" EXCLUDE)
install(FILES include/flex.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT FlexWidgetSDK)
# flex/core/fsm.h is public and includes tinyfsm.hpp; install its actual
# header once and expose it through the Flex::Runtime include interface.
install(FILES "${FLEX_ENGINE_ROOT}/../vendor/tinyfsm/tinyfsm.hpp"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/flex/vendor"
  COMPONENT FlexWidgetSDK)
