# Material Design 3 (M3) Theme for NanoGUI
# This module provides Material Design 3 (Material You) components

cmake_minimum_required(VERSION 3.13)

project(nanogui_m3_theme
    VERSION 1.0.0
    DESCRIPTION "Material Design 3 (Material You) theme for NanoGUI"
    LANGUAGES CXX
)


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


add_library(nanogui_m3_theme STATIC ${M3_THEME_SOURCES} ${M3_THEME_HEADERS})

# Add alias for consistent naming
add_library(NanoGUI::M3Theme ALIAS nanogui_m3_theme)

# Include directories
target_include_directories(nanogui_m3_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "M3 Theme: Linking to nanogui target")
# Link to nanogui (which provides nanogui symbols) and nanovg directly
# Don't define NANOGUI_BUILD - we want to import symbols from nanogui DLL
target_link_libraries(nanogui_m3_theme PUBLIC nanogui nanovg::nanovg)
set_target_properties( nanogui_m3_theme PROPERTIES FOLDER ${folder_name}/)
# Print summary
message(STATUS "")
message(STATUS "Material Design 3 Theme Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${M3_THEME_BUILD_SHARED}")
message(STATUS "  Specification: https://m3.material.io")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
