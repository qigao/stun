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

# FlexUI source files
set(FLEXUI_SOURCES
    src/widget.cpp
    src/screen.cpp
    src/button.cpp
    src/checkbox.cpp
    src/textbox.cpp
    src/label.cpp
    src/progressbar.cpp
    src/slider.cpp
    src/radiobutton.cpp
    src/divider.cpp
    src/imageview.cpp
    src/tabbar.cpp
    src/dropdown.cpp
    src/jsengine.cpp
    src/switch.cpp
    src/rating.cpp
    src/searchbox.cpp
    src/spinner.cpp
    src/alert.cpp
    src/badge.cpp
    src/card.cpp
    src/avatar.cpp
    src/chip.cpp
    src/toast.cpp
    src/breadcrumb.cpp
    src/pagination.cpp
    src/menu.cpp
    src/modal.cpp
    src/table.cpp
    src/iconbutton.cpp
    src/calendar.cpp
    src/colorpicker.cpp
    src/snackbar.cpp
    src/tablist.cpp
    src/scrollview.cpp
    src/font_manager.cpp
    src/sparkline.cpp
)

set(FLEXUI_HEADERS
    include/flexui.h
    include/flexui/widget.h
    include/flexui/screen.h
    include/flexui/snackbar.h
    include/flexui/toast.h
    include/flexui/menu.h
    include/flexui/pagination.h
    include/flexui/searchbox.h
    include/flexui/rating.h
)
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

find_package(OpenGL REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(qjs CONFIG REQUIRED)
target_link_libraries(
  flexui_core
  PUBLIC cssbox SDL3::SDL3 glad::glad OpenGL::GL
  PRIVATE fmtlog::fmtlog pugixml::pugixml  qjs)

# Core-only alias
add_library(FlexUI::Core ALIAS flexui_core)

set_target_properties(flexui_core PROPERTIES FOLDER flexui/)

# ============================================================================
# Examples
# ============================================================================
 
  add_subdirectory(examples)


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
