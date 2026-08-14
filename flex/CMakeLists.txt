# Flex Engine - Visual Core with DSL Parser
cmake_minimum_required(VERSION 3.16)

project(flex LANGUAGES C CXX)
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
  if(TARGET gCanvas::Core)
    add_subdirectory(render/engines/gcanvas)
  else()
    message(STATUS
      "Flex: gCanvas engine disabled because gCanvas::Core is unavailable")
  endif()
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
  if(FLEX_OPENGL_RENDER_ENGINE STREQUAL "gcanvas" AND
     (NOT TARGET flex_render_engine_gcanvas OR NOT TARGET gCanvas::OpenGL))
    message(FATAL_ERROR
      "FLEX_OPENGL_RENDER_ENGINE=gcanvas requires gCanvas OpenGL support")
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

add_subdirectory(examples)
add_subdirectory(tests)

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

install(
  TARGETS ${FLEX_EXPORT_TARGETS}
  EXPORT flex-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(
  DIRECTORY include/flex
  DESTINATION include
  PATTERN "bridge/renderer_*.h" EXCLUDE)
install(FILES include/flex.h DESTINATION include)
