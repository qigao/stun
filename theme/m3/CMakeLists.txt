# Material Design 3 (M3) Theme for NanoGUI
# This module provides Material Design 3 (Material You) components

cmake_minimum_required(VERSION 3.13)

project(nanogui_m3_theme
    VERSION 1.0.0
    DESCRIPTION "Material Design 3 (Material You) theme for NanoGUI"
    LANGUAGES CXX
)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build as shared or static library
option(M3_THEME_BUILD_SHARED "Build M3 Theme as shared library" ON)

# Collect source files
set(M3_THEME_SOURCES
    src/m3_theme.cpp
    src/m3_button.cpp
    src/m3_fab.cpp
    src/m3_card.cpp
    src/m3_text_field.cpp
    src/m3_chip.cpp
    src/m3_switch.cpp
    src/m3_checkbox.cpp
    src/m3_slider.cpp
    src/m3_progress_bar.cpp
    src/m3_dialog.cpp
    src/m3_tooltip.cpp
    src/m3_tooltip_manager.cpp
    src/m3_divider.cpp
    src/m3_badge.cpp
    src/m3_radio.cpp
    src/m3_tabs.cpp
    src/m3_menu.cpp
    src/m3_snackbar.cpp
    src/m3_navigation_bar.cpp
    src/m3_list.cpp
    src/m3_circular_progress.cpp
    src/m3_icon_button.cpp
    src/m3_segmented_button.cpp
    src/m3_top_app_bar.cpp
    src/m3_bottom_sheet.cpp
    src/m3_search_bar.cpp
    src/m3_navigation_drawer.cpp
    src/m3_date_picker.cpp
    src/m3_time_picker.cpp
    src/m3_side_sheet.cpp
    src/m3_autocomplete.cpp
    src/m3_data_table.cpp
    src/m3_extended_fab.cpp
    src/m3_breadcrumb.cpp
)

# Collect header files
set(M3_THEME_HEADERS
    include/nanogui/m3_theme.h
    include/nanogui/m3_button.h
    include/nanogui/m3_fab.h
    include/nanogui/m3_card.h
    include/nanogui/m3_text_field.h
    include/nanogui/m3_chip.h
    include/nanogui/m3_switch.h
    include/nanogui/m3_checkbox.h
    include/nanogui/m3_slider.h
    include/nanogui/m3_progress_bar.h
    include/nanogui/m3_dialog.h
    include/nanogui/m3_tooltip.h
    include/nanogui/m3_tooltip_manager.h
    include/nanogui/m3_divider.h
    include/nanogui/m3_badge.h
    include/nanogui/m3_radio.h
    include/nanogui/m3_tabs.h
    include/nanogui/m3_menu.h
    include/nanogui/m3_snackbar.h
    include/nanogui/m3_icon_button.h
    include/nanogui/m3_segmented_button.h
    include/nanogui/m3_top_app_bar.h
    include/nanogui/m3_bottom_sheet.h
    include/nanogui/m3_search_bar.h
    include/nanogui/m3_navigation_bar.h
    include/nanogui/m3_navigation_drawer.h
    include/nanogui/m3_list.h
    include/nanogui/m3_date_picker.h
    include/nanogui/m3_time_picker.h
    include/nanogui/m3_side_sheet.h
    include/nanogui/m3_autocomplete.h
    include/nanogui/m3_data_table.h
    include/nanogui/m3_extended_fab.h
    include/nanogui/m3_breadcrumb.h
    include/nanogui/m3.h
)

# Create library target
if(M3_THEME_BUILD_SHARED)
    add_library(nanogui_m3_theme SHARED ${M3_THEME_SOURCES} ${M3_THEME_HEADERS})
    target_compile_definitions(nanogui_m3_theme PRIVATE NANOGUI_BUILD)
else()
    add_library(nanogui_m3_theme STATIC ${M3_THEME_SOURCES} ${M3_THEME_HEADERS})
endif()

# Add alias for consistent naming
add_library(NanoGUI::M3Theme ALIAS nanogui_m3_theme)

# Set target properties
set_target_properties(nanogui_m3_theme PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "nanogui_m3"
    EXPORT_NAME M3Theme
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
    set_target_properties(nanogui_m3_theme PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

# Include directories
target_include_directories(nanogui_m3_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against NanoGUI
if(TARGET nanogui)
    message(STATUS "M3 Theme: Linking to nanogui target")
    target_link_libraries(nanogui_m3_theme PUBLIC nanogui)
else()
    message(STATUS "M3 Theme: Finding NanoGUI package")
    find_package(NanoGUI REQUIRED)
    target_link_libraries(nanogui_m3_theme PUBLIC NanoGUI::nanogui)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(nanogui_m3_theme PRIVATE /W4)
else()
    target_compile_options(nanogui_m3_theme PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS nanogui_m3_theme
    EXPORT NanoGUIM3ThemeTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${M3_THEME_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanogui
)

# Export targets
install(EXPORT NanoGUIM3ThemeTargets
    FILE NanoGUIM3ThemeTargets.cmake
    NAMESPACE NanoGUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoGUIM3Theme
)

# Create config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/NanoGUIM3ThemeConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIM3ThemeConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoGUIM3Theme
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIM3ThemeConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIM3ThemeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NanoGUIM3ThemeConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoGUIM3Theme
)

# Print summary
message(STATUS "")
message(STATUS "Material Design 3 Theme Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${M3_THEME_BUILD_SHARED}")
message(STATUS "  Specification: https://m3.material.io")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
