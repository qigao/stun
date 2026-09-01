cmake_minimum_required(VERSION 3.20)

project(FlexUIPluginEchoExample VERSION 1.0.0 LANGUAGES C CXX)

option(FLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER
       "Build the installed-package example with MSVC AddressSanitizer" OFF)
option(FLEXUI_EXAMPLE_BUILD_HOST
       "Build the C++ PluginHost consumer in addition to the pure C DLL" ON)

if(FLEXUI_EXAMPLE_BUILD_HOST)
  find_package(
    FlexUI 1.0 CONFIG REQUIRED
    COMPONENTS PluginSDK PluginHost)
else()
  find_package(FlexUI 1.0 CONFIG REQUIRED COMPONENTS PluginSDK)
endif()

add_library(flexui_echo_plugin MODULE plugin.c)
target_link_libraries(flexui_echo_plugin PRIVATE FlexUI::PluginSDK)
target_compile_definitions(flexui_echo_plugin PRIVATE FLEXUI_PLUGIN_BUILD)
set_target_properties(
  flexui_echo_plugin
  PROPERTIES C_STANDARD 11
             C_STANDARD_REQUIRED ON
             C_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON)

if(FLEXUI_EXAMPLE_BUILD_HOST)
  add_executable(flexui_plugin_consumer host.cpp)
  target_compile_features(flexui_plugin_consumer PRIVATE cxx_std_17)
  target_link_libraries(flexui_plugin_consumer PRIVATE FlexUI::PluginHost)
  target_compile_definitions(
    flexui_plugin_consumer
    PRIVATE FLEXUI_EXAMPLE_PLUGIN_PATH="$<TARGET_FILE:flexui_echo_plugin>")
  add_dependencies(flexui_plugin_consumer flexui_echo_plugin)
endif()

if(FLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER)
  if(NOT MSVC)
    message(FATAL_ERROR
            "FLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER currently requires MSVC")
  endif()
  target_compile_options(
    flexui_echo_plugin PRIVATE /fsanitize=address)
  target_link_options(flexui_echo_plugin PRIVATE /INCREMENTAL:NO)
  if(FLEXUI_EXAMPLE_BUILD_HOST)
    target_compile_options(
      flexui_plugin_consumer PRIVATE /fsanitize=address)
    target_link_options(flexui_plugin_consumer PRIVATE /INCREMENTAL:NO)
  endif()
endif()

set_target_properties(
  flexui_echo_plugin
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
             ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
if(FLEXUI_EXAMPLE_BUILD_HOST)
  set_target_properties(
    flexui_plugin_consumer
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(FLEXUI_EXAMPLE_BUILD_HOST)
  enable_testing()
  add_test(NAME flexui_plugin_install_consumer COMMAND flexui_plugin_consumer)
endif()
