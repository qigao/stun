cmake_minimum_required(VERSION 3.13)

project(nanogui_fluent_web_theme
    VERSION 1.0.0
    DESCRIPTION "Microsoft Fluent 2 web theme for NanoGUI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB FLUENT_WEB_THEME_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB FLUENT_WEB_THEME_HEADERS CONFIGURE_DEPENDS "include/nanogui/*.h")

add_library(nanogui_fluent_web_theme STATIC
    ${FLUENT_WEB_THEME_SOURCES}
    ${FLUENT_WEB_THEME_HEADERS}
)

add_library(NanoGUI::FluentWebTheme ALIAS nanogui_fluent_web_theme)

target_compile_definitions(nanogui_fluent_web_theme PRIVATE NANOGUI_BUILD)

target_include_directories(nanogui_fluent_web_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nanogui_fluent_web_theme
    PUBLIC
        nanogui
        nanovg::nanovg
)
set_target_properties( nanogui_fluent_web_theme PROPERTIES FOLDER ${folder_name}/)