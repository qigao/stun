cmake_minimum_required(VERSION 3.20)

project(FlexUIPluginEchoExample VERSION 1.0.0 LANGUAGES C)

option(FLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER
       "Build the installed-package example with AddressSanitizer" OFF)
option(FLEXUI_EXAMPLE_ENABLE_UNDEFINED_SANITIZER
       "Build the installed-package example with UndefinedBehaviorSanitizer" OFF)
option(FLEXUI_EXAMPLE_BUILD_HOST
       "Build the C++ PluginHost consumer in addition to the pure C DLL" ON)

if(FLEXUI_EXAMPLE_BUILD_HOST)
  enable_language(CXX)
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
  file(GENERATE
       OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installed-component-$<CONFIG>.txt"
       CONTENT "FlexUIConfig=${FlexUI_CONFIG}\nIncludes=$<TARGET_PROPERTY:FlexUI::PluginSDK,INTERFACE_INCLUDE_DIRECTORIES>\nServices=$<TARGET_FILE:FlexUI::Services>\nHost=$<TARGET_FILE:FlexUI::PluginHost>\nCore=$<TARGET_FILE:Salts::Core>\nToml=$<TARGET_FILE:Salts::TomlParser>\n")
endif()

if(FLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER OR
   FLEXUI_EXAMPLE_ENABLE_UNDEFINED_SANITIZER)
  set(_flexui_example_sanitizer_targets flexui_echo_plugin)
  if(FLEXUI_EXAMPLE_BUILD_HOST)
    list(APPEND _flexui_example_sanitizer_targets flexui_plugin_consumer)
  endif()

  if(MSVC)
    if(FLEXUI_EXAMPLE_ENABLE_UNDEFINED_SANITIZER)
      message(FATAL_ERROR "UndefinedBehaviorSanitizer is not supported by MSVC")
    endif()
    foreach(_flexui_example_target IN LISTS _flexui_example_sanitizer_targets)
      target_compile_options(${_flexui_example_target} PRIVATE /fsanitize=address)
      target_link_options(${_flexui_example_target} PRIVATE /INCREMENTAL:NO)
    endforeach()
  elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" AND
         (NOT FLEXUI_EXAMPLE_BUILD_HOST OR
          CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$"))
    set(_flexui_example_sanitizers "")
    if(FLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER)
      list(APPEND _flexui_example_sanitizers address)
    endif()
    if(FLEXUI_EXAMPLE_ENABLE_UNDEFINED_SANITIZER)
      list(APPEND _flexui_example_sanitizers undefined)
    endif()
    list(JOIN _flexui_example_sanitizers "," _flexui_example_sanitizer_flags)
    foreach(_flexui_example_target IN LISTS _flexui_example_sanitizer_targets)
      target_compile_options(
        ${_flexui_example_target}
        PRIVATE "-fsanitize=${_flexui_example_sanitizer_flags}"
                -fno-omit-frame-pointer)
      target_link_options(
        ${_flexui_example_target}
        PRIVATE "-fsanitize=${_flexui_example_sanitizer_flags}")
    endforeach()
  else()
    message(FATAL_ERROR
            "The requested example sanitizers require MSVC, GCC, or Clang")
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
