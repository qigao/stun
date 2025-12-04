cmake_minimum_required(VERSION 3.15)

project(cssbox LANGUAGES C CXX)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
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

# Find pugixml for SVG XML parsing
find_package(pugixml CONFIG REQUIRED)
message(STATUS "NanoVG CSS: Found pugixml for SVG XML parsing")

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

# Generate CSS gradient lexer from re2c
set(GRADIENT_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/css_gradient_lexer.re)
set(GRADIENT_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/css_gradient_lexer_gen.c)

add_custom_command(
  OUTPUT ${GRADIENT_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${GRADIENT_LEXER_GEN} ${GRADIENT_LEXER_RE}
  DEPENDS ${GRADIENT_LEXER_RE}
  COMMENT "Generating CSS gradient lexer with re2c"
  VERBATIM
)

# Generate CSS calc() lexer from re2c
set(CALC_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/css_calc_lexer.re)
set(CALC_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/css_calc_lexer_gen.c)

add_custom_command(
  OUTPUT ${CALC_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${CALC_LEXER_GEN} ${CALC_LEXER_RE}
  DEPENDS ${CALC_LEXER_RE}
  COMMENT "Generating CSS calc() lexer with re2c"
  VERBATIM
)

set(SVG_PATH_SOURCES src/cssbox_svg_path.cpp ${SVG_LEXER_GEN})
set(GRADIENT_SOURCES ${GRADIENT_LEXER_GEN})
set(CALC_SOURCES ${CALC_LEXER_GEN})

# CSS extension sources
set(cssbox_SOURCES
    src/cssbox.cpp
    src/cssbox_stylesheet.cpp
    src/cssbox_painter.cpp
    src/cssbox_utils.cpp
    src/cssbox_keyframes.cpp
    src/cssbox_filters.cpp
    src/cssbox_quadtree.cpp
    src/lexbor_css_parser.cpp
    src/cssbox_svg_xml.cpp
    ${SVG_PATH_SOURCES}
    ${GRADIENT_SOURCES}
    ${CALC_SOURCES}
    src/cssbox_path_sampler.cpp)

set(cssbox_HEADERS
 include/cssbox.h
 include/cssbox_internal.h
 include/cssbox_types.h
 include/cssbox_conversion.h
 include/cssbox_background.h
 include/cssbox_filters.h
 include/cssbox_quadtree.h
 include/cssbox_svg_xml.h
 include/lexbor_css_parser.h
 include/lexbor_wrapper.h
 )

# Create CSS extension library
add_library(cssbox STATIC ${cssbox_SOURCES} ${cssbox_HEADERS})

# C++ standard
set_target_properties(
  cssbox
  PROPERTIES CXX_STANDARD 17
             CXX_STANDARD_REQUIRED ON
             FOLDER cssbox/)

# Include directories
target_include_directories(
  cssbox
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_BINARY_DIR}
          ${LEXBOR_INCLUDE_DIR})

# Link dependencies
target_link_libraries(
  cssbox
  PUBLIC nanovg   # Use internal NanoVG build
  OpenGL::GL
  glad::glad TurboMemory
  PRIVATE ${LEXBOR_LIBRARY}
  pugixml::pugixml
  fmtlog)

# Platform-specific memory profiling libraries
if(WIN32)
  target_link_libraries(cssbox PRIVATE psapi)
endif()

# Create an alias for compatibility
add_library(nanovg::css ALIAS cssbox)

message(STATUS "NanoVG CSS extension configured")

add_subdirectory(tests)
add_subdirectory(examples)
