cmake_minimum_required(VERSION 3.15)

project(nanovg_css LANGUAGES C CXX)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
# ============================================================================
# Build NanoVG internally
# ============================================================================

# NanoVG source files (embedded in this module)
set(NANOVG_SOURCES
    nanovg/nanovg.c
    nanovg/nanovg.h
    nanovg/nanovg_rough.c
    nanovg/nanovg_rough.h
    nanovg/fontstash.h
    nanovg/stb_image.h
    nanovg/stb_truetype.h
    nanovg/nanovg_gl.h
    nanovg/nanovg_gl_utils.h)

# Build NanoVG as a static library
add_library(nanovg_internal STATIC ${NANOVG_SOURCES})

# Set C standard for NanoVG
set_target_properties(
  nanovg_internal
  PROPERTIES C_STANDARD 99
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             FOLDER nanovg_css/)

# NanoVG include directories
target_include_directories(
  nanovg_internal PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nanovg>)

# Platform-specific definitions for NanoVG
if(WIN32)
  target_compile_definitions(nanovg_internal PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Suppress common warnings in nanovg source
if(MSVC)
  target_compile_options(nanovg_internal PRIVATE /wd4005 /wd4456 /wd4457)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(nanovg_internal PRIVATE -Wno-unused-result)
endif()

# ============================================================================
# NanoVG CSS Extension
# ============================================================================

# Find lexbor for CSS parsing
find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/core/base.h)
find_library(LEXBOR_LIBRARY NAMES lexbor)

if(NOT LEXBOR_INCLUDE_DIR)
  message(
    FATAL_ERROR
      "NanoVG CSS: Lexbor include directory not found. Please install via vcpkg: vcpkg install lexbor"
  )
endif()

if(NOT LEXBOR_LIBRARY)
  message(
    FATAL_ERROR
      "NanoVG CSS: Lexbor library not found. Please install via vcpkg: vcpkg install lexbor"
  )
endif()

message(STATUS "NanoVG CSS: Found Lexbor library: ${LEXBOR_LIBRARY}")
message(STATUS "NanoVG CSS: Found Lexbor include: ${LEXBOR_INCLUDE_DIR}")

# ============================================================================
# Generate SVG Path Lexer with re2c
# ============================================================================

# Find re2c
find_program(RE2C_EXECUTABLE re2c)
if(NOT RE2C_EXECUTABLE)
  message(FATAL_ERROR
    "NanoVG CSS: re2c not found - required for SVG path parser.\n"
    "Install re2c:\n"
    "  Ubuntu/Debian: sudo apt install re2c\n"
    "  macOS: brew install re2c\n"
    "  Windows (MSYS2): pacman -S re2c\n"
    "  Or from source: https://re2c.org/")
endif()

message(STATUS "NanoVG CSS: Found re2c: ${RE2C_EXECUTABLE}")

# Generate lexer from re2c
set(SVG_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/svg_path_lexer.re)
set(SVG_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/svg_path_lexer_gen.c)

add_custom_command(
  OUTPUT ${SVG_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${SVG_LEXER_GEN} ${SVG_LEXER_RE}
  DEPENDS ${SVG_LEXER_RE}
  COMMENT "Generating SVG path lexer with re2c"
  VERBATIM
)

set(SVG_PATH_SOURCES src/nanovg_css_svg_path.cpp ${SVG_LEXER_GEN})

# CSS extension sources
set(NANOVG_CSS_SOURCES
    src/nanovg_css.cpp
    src/nanovg_css_stylesheet.cpp
    src/nanovg_css_painter.cpp
    src/nanovg_css_utils.cpp
    src/nanovg_css_keyframes.cpp
    src/nanovg_css_flexbox.cpp
    src/nanovg_css_grid.cpp
    src/nanovg_css_filters.cpp
    src/nanovg_css_quadtree.cpp
    src/lexbor_css_parser.cpp
    ${SVG_PATH_SOURCES}
    src/nanovg_css_path_sampler.cpp)

set(NANOVG_CSS_HEADERS
 include/nanovg_css.h
 include/nanovg_css_internal.h
 include/nanovg_css_types.h       
 include/nanovg_css_conversion.h     
 include/nanovg_css_background.h
 include/nanovg_css_filters.h
 include/nanovg_css_quadtree.h
 include/lexbor_css_parser.h
 include/lexbor_wrapper.h
 )

# Create CSS extension library
add_library(nanovg_css STATIC ${NANOVG_CSS_SOURCES} ${NANOVG_CSS_HEADERS})
add_library(nanovg::css ALIAS nanovg_css)

# C++ standard
set_target_properties(
  nanovg_css
  PROPERTIES CXX_STANDARD 17
             CXX_STANDARD_REQUIRED ON
             FOLDER nanovg_css/)

# Include directories
target_include_directories(
  nanovg_css
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_BINARY_DIR}
          ${LEXBOR_INCLUDE_DIR})

# Link dependencies
target_link_libraries(
  nanovg_css  
  PUBLIC nanovg_internal   # Use internal NanoVG build
  OpenGL::GL
  glad::glad
  PRIVATE ${LEXBOR_LIBRARY} fmtlog)

# Create an alias for compatibility
add_library(nanovg::css ALIAS nanovg_css)

message(STATUS "NanoVG CSS extension configured")

add_subdirectory(tests)
add_subdirectory(examples)
