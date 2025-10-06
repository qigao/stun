# Fluent Design Theme for NanoGUI
# Based on Microsoft Fluent Design System
# https://fluent2.microsoft.design/

cmake_minimum_required(VERSION 3.13)

project(nanogui_fluent_theme
    VERSION 1.0.0
    DESCRIPTION "Microsoft Fluent Design System for NanoGUI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FLUENT_THEME_BUILD_SHARED "Build Fluent Theme as shared library" ON)

# Collect all source files
file(GLOB FLUENT_THEME_SOURCES "src/*.cpp")
file(GLOB FLUENT_THEME_HEADERS "include/nanogui/*.h")

# Create library target
if(FLUENT_THEME_BUILD_SHARED)
    add_library(nanogui_fluent_theme SHARED ${FLUENT_THEME_SOURCES} ${FLUENT_THEME_HEADERS})
    target_compile_definitions(nanogui_fluent_theme PRIVATE NANOGUI_BUILD)
else()
    add_library(nanogui_fluent_theme STATIC ${FLUENT_THEME_SOURCES} ${FLUENT_THEME_HEADERS})
endif()

add_library(NanoGUI::FluentTheme ALIAS nanogui_fluent_theme)

set_target_properties(nanogui_fluent_theme PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "nanogui_fluent"
    EXPORT_NAME FluentTheme
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# For multi-config generators
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(nanogui_fluent_theme PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

target_include_directories(nanogui_fluent_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(TARGET nanogui)
    message(STATUS "Fluent Theme: Linking to nanogui target")
    target_link_libraries(nanogui_fluent_theme PUBLIC nanogui)
else()
    message(STATUS "Fluent Theme: Finding NanoGUI package")
    find_package(NanoGUI REQUIRED)
    target_link_libraries(nanogui_fluent_theme PUBLIC NanoGUI::nanogui)
endif()

if(MSVC)
    target_compile_options(nanogui_fluent_theme PRIVATE /W4)
else()
    target_compile_options(nanogui_fluent_theme PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "")
message(STATUS "Fluent Design Theme Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${FLUENT_THEME_BUILD_SHARED}")
message(STATUS "  Design System: Microsoft Fluent Design")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
