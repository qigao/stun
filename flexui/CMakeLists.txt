cmake_minimum_required(VERSION 3.15)
project(
  FlexUI
  VERSION 2.0.0
  LANGUAGES C CXX)

# Define NANOGUI_USE_SDL3 for SDL3 backend
add_definitions(-DNANOGUI_USE_OPENGL)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Options
option(FLEXUI_BUILD_EXAMPLES "Build FlexUI examples" ON)
option(FLEXUI_BUILD_TESTS "Build FlexUI unit tests" ON)

# Collect all FlexUI source files
file(GLOB_RECURSE FLEXUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE FLEXUI_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
# ============================================================================
# Generate embedded font resources
# ============================================================================

# Glob up resource files (fonts)
file(GLOB resources "${CMAKE_SOURCE_DIR}/resources/*.ttf")

# Concatenate resource files into a comma separated string
string(REGEX REPLACE "([^\\]|^);" "\\1," resources_string "${resources}")
string(REGEX REPLACE "[\\](.)" "\\1" resources_string "${resources_string}")

# Create command line for running bin2c cmake script
set(bin2c_cmdline
    -DOUTPUT_C=flexui_resources.cpp -DOUTPUT_H=flexui_resources.h
    "-DINPUT_FILES=${resources_string}" -P
    "${CMAKE_SOURCE_DIR}/resources/bin2c.cmake")

# Run bin2c on resource files
add_custom_command(
  OUTPUT flexui_resources.cpp flexui_resources.h
  COMMAND ${CMAKE_COMMAND} ARGS ${bin2c_cmdline}
  DEPENDS ${resources}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Running bin2c for FlexUI resources"
  VERBATIM)

# ============================================================================
# FlexUI Core Library
# ============================================================================

add_library(
  flexui_core STATIC
  ${FLEXUI_SOURCES} ${FLEXUI_HEADER}
  # Embedded resources
  ${CMAKE_CURRENT_BINARY_DIR}/flexui_resources.cpp)

target_include_directories(
  flexui_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
         ${CMAKE_CURRENT_BINARY_DIR} # For flexui_resources.h
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/nanogui/include> # Add nanogui's public headers
)

# Link to dependencies: - nanovg_css: CSS rendering engine (provides
# nanovg_css.h and NanoVG) - SDL3: Window/event management - fmtlog: Logging
# (optional, for debugging) - OpenGL: Required for NanoVG rendering backend
find_package(OpenGL REQUIRED)
target_link_libraries(
  flexui_core
  PUBLIC nanovg_css SDL3::SDL3 glad::glad OpenGL::GL
  PRIVATE fmtlog::fmtlog)

# Core-only alias
add_library(FlexUI::Core ALIAS flexui_core)

set_target_properties(flexui_core PROPERTIES FOLDER flexui/)

# ============================================================================
# Examples
# ============================================================================

if(FLEXUI_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# ============================================================================
# Unit Tests
# ============================================================================

if(FLEXUI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "FlexUI Configuration:")
message(STATUS "  Version:              ${PROJECT_VERSION}")
message(STATUS "  Build examples:       ${FLEXUI_BUILD_EXAMPLES}")
message(STATUS "  Build tests:          ${FLEXUI_BUILD_TESTS}")
message(STATUS "")
