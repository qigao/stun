cmake_minimum_required(VERSION 3.20)

project(FlexUIInstalledComponentConsumer LANGUAGES NONE)

set(FLEXUI_CONSUMER_COMPONENT "PluginSDK" CACHE STRING
    "The installed component to consume independently: PluginSDK or Services")
set_property(CACHE FLEXUI_CONSUMER_COMPONENT PROPERTY STRINGS PluginSDK Services)

if(FLEXUI_CONSUMER_COMPONENT STREQUAL "PluginSDK")
  enable_language(C)
  find_package(FlexUI 1.0 CONFIG REQUIRED COMPONENTS PluginSDK)
  if(TARGET FlexUI::Services)
    message(FATAL_ERROR "PluginSDK unexpectedly imported Services")
  endif()
  add_executable(flexui_installed_sdk_consumer sdk.c)
  target_link_libraries(flexui_installed_sdk_consumer PRIVATE FlexUI::PluginSDK)
  set_target_properties(flexui_installed_sdk_consumer
                        PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
                                   C_EXTENSIONS OFF)
  set(_flexui_consumer flexui_installed_sdk_consumer)
elseif(FLEXUI_CONSUMER_COMPONENT STREQUAL "Services")
  enable_language(CXX)
  find_package(FlexUI 1.0 CONFIG REQUIRED COMPONENTS Services)
  add_executable(flexui_installed_services_consumer services.cpp)
  target_link_libraries(flexui_installed_services_consumer PRIVATE FlexUI::Services)
  target_compile_features(flexui_installed_services_consumer PRIVATE cxx_std_17)
  set_target_properties(flexui_installed_services_consumer
                        PROPERTIES CXX_EXTENSIONS OFF)
  set(_flexui_consumer flexui_installed_services_consumer)
else()
  message(FATAL_ERROR
          "FLEXUI_CONSUMER_COMPONENT must be PluginSDK or Services")
endif()

if(TARGET FlexUI::PluginHost OR TARGET Salts::Core OR TARGET Salts::TomlParser)
  message(FATAL_ERROR
          "${FLEXUI_CONSUMER_COMPONENT} unexpectedly imported host dependencies")
endif()

set(_flexui_consumer_evidence
    "FlexUIConfig=${FlexUI_CONFIG}\nIncludes=$<TARGET_PROPERTY:FlexUI::${FLEXUI_CONSUMER_COMPONENT},INTERFACE_INCLUDE_DIRECTORIES>\n")
if(FLEXUI_CONSUMER_COMPONENT STREQUAL "Services")
  string(APPEND _flexui_consumer_evidence
         "Library=$<TARGET_FILE:FlexUI::Services>\n")
endif()
file(GENERATE
     OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installed-component-$<CONFIG>.txt"
     CONTENT "${_flexui_consumer_evidence}")

enable_testing()
add_test(NAME ${_flexui_consumer} COMMAND ${_flexui_consumer})
