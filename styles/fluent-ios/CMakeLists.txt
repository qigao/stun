cmake_minimum_required(VERSION 3.13)

project(nanogui_fluent_ios_theme
    VERSION 0.1.0
    DESCRIPTION "Fluent UI iOS inspired components for NanoGUI"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FLUENT_IOS_SOURCES
    src/fluent_ios_theme.cpp
    src/fluent_ios_button.cpp
    src/fluent_ios_navigation_bar.cpp
    src/fluent_ios_segmented_control.cpp
    src/fluent_ios_list_cell.cpp
    src/fluent_ios_switch.cpp
    src/fluent_ios_text_field.cpp)

set(FLUENT_IOS_HEADERS
    include/nanogui/fluent_ios_theme.h
    include/nanogui/fluent_ios_button.h
    include/nanogui/fluent_ios_navigation_bar.h
    include/nanogui/fluent_ios_segmented_control.h
    include/nanogui/fluent_ios_list_cell.h
    include/nanogui/fluent_ios_switch.h
    include/nanogui/fluent_ios_text_field.h)

add_library(nanogui_fluent_ios_theme STATIC
    ${FLUENT_IOS_SOURCES}
    ${FLUENT_IOS_HEADERS})

target_compile_definitions(nanogui_fluent_ios_theme PRIVATE NANOGUI_BUILD)

add_library(NanoGUI::FluentIOSTheme ALIAS nanogui_fluent_ios_theme)

target_include_directories(nanogui_fluent_ios_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

message(STATUS "Fluent iOS Theme: Linking to nanogui target")
target_link_libraries(nanogui_fluent_ios_theme PUBLIC nanogui nanovg::nanovg)
set_target_properties( nanogui_fluent_ios_theme PROPERTIES FOLDER ${folder_name}/)