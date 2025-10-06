# Apple Human Interface Guidelines Theme for NanoGUI
# This module provides Apple HIG components for macOS/iOS style interfaces

cmake_minimum_required(VERSION 3.13)

project(nanogui_apple_theme
    VERSION 1.0.0
    DESCRIPTION "Apple Human Interface Guidelines theme for NanoGUI"
    LANGUAGES CXX
)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build as shared or static library
option(APPLE_THEME_BUILD_SHARED "Build Apple Theme as shared library" ON)

# Collect source files
set(APPLE_THEME_SOURCES
    src/apple_theme.cpp
    src/apple_button.cpp
    src/apple_toggle.cpp
    src/apple_checkbox.cpp
    src/apple_slider.cpp
    src/apple_text_field.cpp
    src/apple_list.cpp
    src/apple_card.cpp
    src/apple_segmented_control.cpp
    src/apple_progress.cpp
    src/apple_sheet.cpp
    src/apple_alert.cpp
    src/apple_navigation_bar.cpp
    src/apple_tab_bar.cpp
    src/apple_sidebar.cpp
    src/apple_badge.cpp
    src/apple_stepper.cpp
    src/apple_date_picker.cpp
    src/apple_popover.cpp
    src/apple_toolbar.cpp
    src/apple_animation.cpp
    src/apple_effects.cpp
)

# Collect header files
set(APPLE_THEME_HEADERS
    include/nanogui/apple_theme.h
    include/nanogui/apple_button.h
    include/nanogui/apple_toggle.h
    include/nanogui/apple_checkbox.h
    include/nanogui/apple_slider.h
    include/nanogui/apple_text_field.h
    include/nanogui/apple_list.h
    include/nanogui/apple_card.h
    include/nanogui/apple_segmented_control.h
    include/nanogui/apple_progress.h
    include/nanogui/apple_sheet.h
    include/nanogui/apple_alert.h
    include/nanogui/apple_navigation_bar.h
    include/nanogui/apple_tab_bar.h
    include/nanogui/apple_sidebar.h
    include/nanogui/apple_badge.h
    include/nanogui/apple_stepper.h
    include/nanogui/apple_date_picker.h
    include/nanogui/apple_popover.h
    include/nanogui/apple_toolbar.h
    include/nanogui/apple_animation.h
    include/nanogui/apple_effects.h
    include/nanogui/apple.h
)

# Create library target
if(APPLE_THEME_BUILD_SHARED)
    add_library(nanogui_apple_theme SHARED ${APPLE_THEME_SOURCES} ${APPLE_THEME_HEADERS})
    target_compile_definitions(nanogui_apple_theme PRIVATE NANOGUI_BUILD)
else()
    add_library(nanogui_apple_theme STATIC ${APPLE_THEME_SOURCES} ${APPLE_THEME_HEADERS})
endif()

# Add alias for consistent naming
add_library(NanoGUI::AppleTheme ALIAS nanogui_apple_theme)

# Set target properties
set_target_properties(nanogui_apple_theme PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "nanogui_apple"
    EXPORT_NAME AppleTheme
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
    set_target_properties(nanogui_apple_theme PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

# Include directories
target_include_directories(nanogui_apple_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against NanoGUI
if(TARGET nanogui)
    message(STATUS "Apple Theme: Linking to nanogui target")
    target_link_libraries(nanogui_apple_theme PUBLIC nanogui)
else()
    message(STATUS "Apple Theme: Finding NanoGUI package")
    find_package(NanoGUI REQUIRED)
    target_link_libraries(nanogui_apple_theme PUBLIC NanoGUI::nanogui)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(nanogui_apple_theme PRIVATE /W4)
else()
    target_compile_options(nanogui_apple_theme PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS nanogui_apple_theme
    EXPORT NanoGUIAppleThemeTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${APPLE_THEME_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanogui
)

# Export targets
install(EXPORT NanoGUIAppleThemeTargets
    FILE NanoGUIAppleThemeTargets.cmake
    NAMESPACE NanoGUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoGUIAppleTheme
)

# Create config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/NanoGUIAppleThemeConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIAppleThemeConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoGUIAppleTheme
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIAppleThemeConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIAppleThemeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIAppleThemeConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoGUIAppleTheme
)

# Print summary
message(STATUS "")
message(STATUS "Apple Human Interface Guidelines Theme Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${APPLE_THEME_BUILD_SHARED}")
message(STATUS "  Specification: https://developer.apple.com/design/")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
