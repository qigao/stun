cmake_minimum_required(VERSION 3.20)

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

project(
  nanogui
  DESCRIPTION "NanoGUI"
  LANGUAGES CXX C)

include(CMakeOptions.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Compiler flags are defined in presets/CompilerFlags.json Use C++17,
# visibility=hidden by default
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Use GLAD from vcpkg
find_package(glad CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(base64 CONFIG REQUIRED)
find_package(qjs CONFIG REQUIRED)
# Use robin-map from vcpkg
find_package(tsl-robin-map CONFIG REQUIRED)
find_package(TurboNet CONFIG REQUIRED PATHS ${EXTERNAL_ROOT}/turbonet)

# Output files directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Add vendor libraries before creating nanogui target
add_subdirectory(vendor)
add_subdirectory(flex)
add_subdirectory(flexUI)
add_subdirectory(meta_editor)
add_subdirectory(tango)
add_subdirectory(flexui_designer)
add_subdirectory(FlexPlayer)
add_subdirectory(charts)

