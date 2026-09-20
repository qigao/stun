cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

project(
  nanogui
  DESCRIPTION "NanoGUI"
  LANGUAGES CXX C)

include(CTest)
include(CMakeOptions.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Compiler flags are defined in presets/CompilerFlags.json Use C++17,
# visibility=hidden by default
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(NANOGUI_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NANOGUI_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${NANOGUI_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${NANOGUI_OUTPUT_DIR}")

find_package(base64 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(TurboUtils CONFIG REQUIRED PATHS "${TURBO_UTILS_ROOT}")
find_package(TurboParser CONFIG REQUIRED PATHS "${TURBOPARSER_ROOT}")
find_package(Salts CONFIG REQUIRED PATHS "${SALTS_ROOT}")
find_package(SaltsUtils CONFIG REQUIRED PATHS "${SALTS_UTILS_ROOT}")
find_package(TurboNet CONFIG REQUIRED PATHS "${TURBONET_ROOT}")

# Cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Sanitizers)
add_subdirectory(tools)
include(FindTools)
# Add vendor libraries before creating nanogui target
add_subdirectory(vendor)
add_subdirectory(flex)
add_subdirectory(flexUI)
add_subdirectory(TextEdit)
add_subdirectory(meta_editor)
add_subdirectory(tango)
add_subdirectory(flexui_designer)
# FlexPlayer's legacy SDL software compositor depends on the retired ThorVG
# renderer and an external FFmpeg SDK. Keep it outside the supported build
# until it has a native GPU compositor.
add_subdirectory(charts)

function(nanogui_set_target_output_dir target_name)
  if(NOT TARGET "${target_name}")
    return()
  endif()

  get_target_property(target_type "${target_name}" TYPE)
  if(NOT target_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY)$")
    return()
  endif()

  set_target_properties(
    "${target_name}"
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${NANOGUI_OUTPUT_DIR}"
      LIBRARY_OUTPUT_DIRECTORY "${NANOGUI_OUTPUT_DIR}"
      ARCHIVE_OUTPUT_DIRECTORY "${NANOGUI_OUTPUT_DIR}")

  set(output_configs Debug Release RelWithDebInfo MinSizeRel)
  if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND output_configs ${CMAKE_CONFIGURATION_TYPES})
  elseif(CMAKE_BUILD_TYPE)
    list(APPEND output_configs ${CMAKE_BUILD_TYPE})
  endif()
  list(REMOVE_DUPLICATES output_configs)

  foreach(output_config IN LISTS output_configs)
    string(TOUPPER "${output_config}" output_config_upper)
    set_target_properties(
      "${target_name}"
      PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${output_config_upper} "${NANOGUI_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_${output_config_upper} "${NANOGUI_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_${output_config_upper} "${NANOGUI_OUTPUT_DIR}")
  endforeach()
endfunction()

function(nanogui_apply_output_dirs source_dir)
  get_property(local_targets DIRECTORY "${source_dir}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(local_target IN LISTS local_targets)
    nanogui_set_target_output_dir("${local_target}")
  endforeach()

  get_property(child_dirs DIRECTORY "${source_dir}" PROPERTY SUBDIRECTORIES)
  foreach(child_dir IN LISTS child_dirs)
    nanogui_apply_output_dirs("${child_dir}")
  endforeach()
endfunction()

nanogui_apply_output_dirs("${CMAKE_CURRENT_SOURCE_DIR}")
