# ============================================================================
# CMake Options Configuration for NanoGUI
# ============================================================================
# This file contains all build options and their default values.
# Include this file early in the root CMakeLists.txt.

# Build type configuration
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

# Detect if NanoGUI is the master project
set(NANOGUI_MASTER_PROJECT OFF)
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(NANOGUI_MASTER_PROJECT ON)
endif()

# ============================================================================
# Default Values for Build Options (platform-specific)
# ============================================================================

# Backend defaults
set(NANOGUI_BUILD_GLFW_DEFAULT OFF)
set(NANOGUI_USE_SDL3_DEFAULT ON)

# GLAD defaults (Windows needs it for OpenGL loading)
if (WIN32)
  set(NANOGUI_BUILD_GLAD_DEFAULT ON)
else()
  set(NANOGUI_BUILD_GLAD_DEFAULT OFF)
endif()

# Shared library defaults
set(NANOGUI_BUILD_SHARED_DEFAULT OFF)
set(NANOGUI_USE_SDL2_DEFAULT OFF)

# Emscripten/WebAssembly specific configuration
if (CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
  set(CMAKE_CXX_COMPILER_ID "Emscripten")
  set(NANOGUI_BUILD_SHARED_DEFAULT OFF)
  set(NANOGUI_BUILD_GLAD_DEFAULT   OFF)
  set(NANOGUI_BUILD_GLFW_DEFAULT  OFF)
  set(NANOGUI_USE_SDL3_DEFAULT     ON)

  set(CMAKE_STATIC_LIBRARY_SUFFIX ".bc")
  set(CMAKE_EXECUTABLE_SUFFIX ".bc")
  set(CMAKE_CXX_CREATE_STATIC_LIBRARY "<CMAKE_CXX_COMPILER> -o <TARGET> <LINK_FLAGS> <OBJECTS>")
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    add_compile_options(-O3 -DNDEBUG)
  endif()
endif()

# ============================================================================
# Build Options (User-configurable)
# ============================================================================

option(NANOGUI_BUILD_EXAMPLES            "Build NanoGUI example application?" ON)
option(NANOGUI_BUILD_SHARED              "Build NanoGUI as a shared library?" ${NANOGUI_BUILD_SHARED_DEFAULT})
option(NANOGUI_BUILD_GLAD                "Build GLAD OpenGL loader library? (needed on Windows)" ${NANOGUI_BUILD_GLAD_DEFAULT})
option(NANOGUI_INSTALL                   "Install NanoGUI on `make install`?" OFF)
option(BUILD_TESTS                       "Build unit tests?" ON)
option(FLEXUI_ENABLE_TURBOSCRIPT         "Build the optional FlexUI TurboScript controller adapter?" OFF)
option(FLEXUI_ENABLE_PLUGINS             "Build the optional FlexUI trusted DLL PluginHost?" OFF)

# Backend selection (mutually exclusive)
include(CMakeDependentOption)
option(NANOGUI_BUILD_GLFW "Use GLFW backend (desktop support)" ${NANOGUI_BUILD_GLFW_DEFAULT})
option(NANOGUI_USE_SDL3 "Use SDL3 backend (cross-platform support)" ${NANOGUI_USE_SDL3_DEFAULT})

# Additional options
option(NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION "Compile Metal shaders at runtime instead of precompiling them while nanogui is built." OFF)
option(NANOGUI_SKIP_STB_IMAGE_IMPLEMENTATION    "Do not compile stb_image's implementation into nanogui. If enabled, user code must do this instead." OFF)

# ============================================================================
# Backend Validation
# ============================================================================

# Validate backend selection - exactly one backend must be enabled
if (NANOGUI_BUILD_GLFW AND NANOGUI_USE_SDL3)
  message(FATAL_ERROR "NanoGUI: Cannot enable both GLFW and SDL3 backends. Choose one:\n"
                      "  -DNANOGUI_BUILD_GLFW=ON   (for GLFW backend)\n"
                      "  -DNANOGUI_USE_SDL3=ON     (for SDL3 backend)")
endif()

if (NOT NANOGUI_BUILD_GLFW AND NOT NANOGUI_USE_SDL3)
  message(FATAL_ERROR "NanoGUI: At least one backend must be enabled:\n"
                      "  -DNANOGUI_BUILD_GLFW=ON   (for GLFW backend)\n"
                      "  -DNANOGUI_USE_SDL3=ON     (for SDL3 backend)")
endif()

# ============================================================================
# Rendering Backend Selection
# ============================================================================

if (NOT NANOGUI_BACKEND)
  if (CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
    set(NANOGUI_BACKEND_DEFAULT "GLES 2")
  elseif (APPLE)
    set(NANOGUI_BACKEND_DEFAULT "Metal")
  elseif (CMAKE_SYSTEM_NAME MATCHES "Linux" OR
          CMAKE_SYSTEM_NAME MATCHES "BSD")
    set(NANOGUI_BACKEND_DEFAULT "OpenGL")
  else()
    set(NANOGUI_BACKEND_DEFAULT "OpenGL")
  endif()

  set(NANOGUI_BACKEND ${NANOGUI_BACKEND_DEFAULT} CACHE STRING "Choose the backend used for rendering (OpenGL/GLES 2/GLES 3/Metal)" FORCE)
endif()

set_property(CACHE NANOGUI_BACKEND PROPERTY STRINGS "OpenGL" "GLES 2" "GLES 3" "Metal")

# ============================================================================
# Status Messages
# ============================================================================

message(STATUS "NanoGUI Options:")
message(STATUS "  Master Project: ${NANOGUI_MASTER_PROJECT}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Examples: ${NANOGUI_BUILD_EXAMPLES}")
message(STATUS "  Build Shared: ${NANOGUI_BUILD_SHARED}")
message(STATUS "  Build GLAD: ${NANOGUI_BUILD_GLAD}")
message(STATUS "  Backend: ${NANOGUI_BACKEND}")
message(STATUS "  Window Backend: ${NANOGUI_BUILD_GLFW} (GLFW) / ${NANOGUI_USE_SDL3} (SDL3)")
