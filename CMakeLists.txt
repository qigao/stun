cmake_minimum_required(VERSION 3.13..3.18)

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0) # macOS Big Sur (released Nov 2020, EOL
                                      # Sep 2023)

# Extract project version from source
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/v1/nanogui/include/nanogui/common.h"
     nanogui_version_defines
     REGEX "#define NANOGUI_VERSION_(MAJOR|MINOR|PATCH) ")

foreach(ver ${nanogui_version_defines})
  if(ver MATCHES [[#define NANOGUI_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$]])
    set(NANOGUI_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endif()
endforeach()

if(NANOGUI_VERSION_PATCH MATCHES [[\.([a-zA-Z0-9]+)$]])
  set(NANOGUI_VERSION_TYPE "${CMAKE_MATCH_1}")
endif()

string(REGEX MATCH "^[0-9]+" NANOGUI_VERSION_PATCH "${NANOGUI_VERSION_PATCH}")

set(NANOGUI_VERSION
    "${NANOGUI_VERSION_MAJOR}.${NANOGUI_VERSION_MINOR}.${NANOGUI_VERSION_PATCH}"
)

message(STATUS "NanoGUI v${NANOGUI_VERSION} ${NANOGUI_VERSION_TYPE}")

project(
  nanogui
  DESCRIPTION "NanoGUI"
  LANGUAGES CXX C
  VERSION ${NANOGUI_VERSION})

# Dependencies are now managed by vcpkg (see vcpkg.json)

# Include build options configuration
include(CMakeOptions.cmake)

# Required libraries, flags, and include files for compiling and linking against
# nanogui (all targets)
set(NANOGUI_EXTRA_INCS "")

if(NANOGUI_BACKEND STREQUAL "OpenGL")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_OPENGL)
  set(NANOGUI_GLOB "resources/*.gl")
  message(STATUS "NanoGUI: using OpenGL backend.")
elseif(NANOGUI_BACKEND STREQUAL "GLES 2")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_GLES -DNANOGUI_GLES_VERSION=2)
  set(NANOGUI_GLOB "resources/*.gles")
  message(STATUS "NanoGUI: using GLES 2 backend.")
elseif(NANOGUI_BACKEND STREQUAL "GLES 3")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_GLES -DNANOGUI_GLES_VERSION=3)
  set(NANOGUI_GLOB "resources/*.gles")
  message(STATUS "NanoGUI: using GLES 3 backend.")
elseif(NANOGUI_BACKEND STREQUAL "Metal")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_METAL)
  set(NANOGUI_GLOB "resources/*.metal")
  message(STATUS "NanoGUI: using Metal backend.")
endif()

# Shared library mode: add dllimport/dllexport flags to all symbols
if(NANOGUI_BUILD_SHARED)
  message(STATUS "NanoGUI: building shared library.")
else()
  message(STATUS "NanoGUI: building static library.")
endif()

# Compiler flags are defined in presets/CompilerFlags.json Use C++17,
# visibility=hidden by default
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Use GLAD from vcpkg
find_package(glad CONFIG REQUIRED)

if(${CMAKE_SYSTEM_NAME} MATCHES "BSD")
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
  if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    include_directories(/usr/X11R6/include/)
    link_directories(/usr/X11R6/lib)
  endif()
endif()

# Adapt to variations in rpath handling on Linux and macOS
if(APPLE)
  set(NANOGUI_ORIGIN "@loader_path")
elseif(UNIX)
  set(NANOGUI_ORIGIN "$ORIGIN")
endif()

# Required core libraries on various platforms
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Emscripten")
  # No dependencies
elseif(WIN32)
  list(APPEND NANOGUI_LIBS opengl32)
elseif(APPLE)
  find_library(cocoa_library Cocoa)
  find_library(iokit_library IOKit)
  list(APPEND NANOGUI_LIBS ${cocoa_library} ${iokit_library})
  mark_as_advanced(cocoa_library iokit_library)

  if(NANOGUI_BACKEND STREQUAL "OpenGL")
    find_library(corevideo_library CoreVideo)
    find_library(opengl_library OpenGL)

    # Disable OpenGL deprecation warnings
    add_definitions(-DGL_SILENCE_DEPRECATION)
    list(APPEND NANOGUI_LIBS ${opengl_library} ${corevideo_library})
    mark_as_advanced(opengl_library corevideo_library)
  else()
    find_library(metal_library Metal)
    list(APPEND NANOGUI_LIBS ${metal_library})
    mark_as_advanced(metal_library)
  endif()
  find_library(quartzcore_library QuartzCore)
  mark_as_advanced(quartzcore_library)
  list(APPEND NANOGUI_EXTRA src/darwin.mm src/autorelease.mm)
  list(APPEND NANOGUI_LIBS ${quartzcore_library})
  set_property(SOURCE src/autorelease.mm PROPERTY COMPILE_FLAGS -fno-objc-arc)
elseif(CMAKE_SYSTEM MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "BSD")
  list(APPEND NANOGUI_LIBS X11 pthread)
  if(NANOGUI_BACKEND STREQUAL "OpenGL")
    list(APPEND NANOGUI_LIBS GL)
  elseif(NANOGUI_BACKEND STREQUAL "GLES 2")
    list(APPEND NANOGUI_LIBS GLESv2)
  elseif(NANOGUI_BACKEND STREQUAL "GLES 3")
    list(APPEND NANOGUI_LIBS GLESv2) # Most systems expose ESv3 functionality
                                     # through GLESv2
  endif()
  if(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    list(APPEND NANOGUI_LIBS rt)
  endif()
  if(CMAKE_SYSTEM MATCHES "Linux")
    list(APPEND NANOGUI_LIBS dl)
  endif()
endif()

# Set library type
if(NANOGUI_BUILD_SHARED)
  set(NANOGUI_LIBRARY_TYPE "SHARED")
else()
  set(NANOGUI_LIBRARY_TYPE "STATIC")
endif()

# Link GLFW backend (legacy desktop support)
if(NANOGUI_BUILD_GLFW)
  set(_nanogui_glfw_target "")
  find_package(glfw3 CONFIG QUIET)
  if(glfw3_FOUND)
    if(TARGET glfw)
      set(_nanogui_glfw_target glfw)
    elseif(TARGET glfw3)
      set(_nanogui_glfw_target glfw3)
    elseif(TARGET glfw::glfw)
      set(_nanogui_glfw_target glfw::glfw)
    endif()
  endif()
  if(_nanogui_glfw_target STREQUAL "")
    find_package(GLFW3 REQUIRED)
    if(TARGET GLFW::GLFW)
      set(_nanogui_glfw_target GLFW::GLFW)
    elseif(DEFINED GLFW3_LIBRARY)
      set(_nanogui_glfw_target ${GLFW3_LIBRARY})
    elseif(DEFINED GLFW3_LIBRARIES)
      set(_nanogui_glfw_target ${GLFW3_LIBRARIES})
    endif()
  endif()
  if(_nanogui_glfw_target STREQUAL "")
    message(
      FATAL_ERROR "NanoGUI: unable to locate a GLFW target to link against.")
  endif()
  message(
    STATUS
      "NanoGUI: GLFW backend linked - Screen available for desktop support.")
endif()

# Link SDL3 backend (cross-platform support)
if(NANOGUI_USE_SDL3)
  set(_nanogui_sdl3_target "")
  find_package(SDL3 CONFIG QUIET)
  if(SDL3_FOUND)
    if(TARGET SDL3::SDL3)
      set(_nanogui_sdl3_target SDL3::SDL3)
    elseif(TARGET SDL3::SDL3-shared)
      set(_nanogui_sdl3_target SDL3::SDL3-shared)
    elseif(TARGET SDL3::SDL3-static)
      set(_nanogui_sdl3_target SDL3::SDL3-static)
    endif()
  endif()
  if(_nanogui_sdl3_target STREQUAL "")
    find_package(SDL3 REQUIRED)
    if(TARGET SDL3::SDL3)
      set(_nanogui_sdl3_target SDL3::SDL3)
    elseif(DEFINED SDL3_LIBRARY)
      set(_nanogui_sdl3_target ${SDL3_LIBRARY})
    elseif(DEFINED SDL3_LIBRARIES)
      set(_nanogui_sdl3_target ${SDL3_LIBRARIES})
    endif()
  endif()
  if(_nanogui_sdl3_target STREQUAL "")
    message(
      FATAL_ERROR "NanoGUI: unable to locate an SDL3 target to link against.")
  endif()
  message(
    STATUS
      "NanoGUI: SDL3 backend linked - Screen available for cross-platform support."
  )
endif()

# Use robin-map from vcpkg
find_package(tsl-robin-map CONFIG REQUIRED)
# Output files directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# For multi-config generators (Visual Studio, Ninja Multi-Config), prevent
# config subdirectories
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}
      "${CMAKE_BINARY_DIR}/bin")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}
      "${CMAKE_BINARY_DIR}/bin")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}
      "${CMAKE_BINARY_DIR}/bin")
endforeach()

# Add vendor libraries before creating nanogui target
add_subdirectory(vendor)
add_subdirectory(nanovg_css)

# Metal backend (Apple platforms only)
if(APPLE AND NANOGUI_BACKEND STREQUAL "Metal")
  add_subdirectory(nanovg_metal)
endif()
 
add_subdirectory(flexui)
