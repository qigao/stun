cmake_minimum_required(VERSION 3.15)
project(flexui_designer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_library(flexui_designer
    src/designer.cpp
    src/widget_palette.cpp
    src/design_canvas.cpp
    src/property_editor.cpp
    src/code_generator.cpp
    src/widget_tree.cpp
    src/menu_bar.cpp
    src/file_dialog.cpp
    src/template_generator.cpp
    src/status_bar.cpp
    src/shortcut_panel.cpp
    src/design_unit.cpp
    src/tab_bar.cpp
    src/history_panel.cpp
    src/search_panel.cpp
    src/script_editor.cpp
)

target_include_directories(flexui_designer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(flexui_designer
    PUBLIC
        meta_editor
        flexUI
        flex
        TurboNet::CoroNet
        nfd
)

add_executable(flexui_designer_app
    examples/main.cpp
)

target_link_libraries(flexui_designer_app
    PRIVATE
        flexui_designer
        flex_backend_opengl
        glad::glad
        OpenGL::GL
        SDL2::SDL2 SDL2::SDL2main
)

set_target_properties(flexui_designer PROPERTIES FOLDER flexui_designer/)
set_target_properties(flexui_designer_app PROPERTIES FOLDER flexui_designer/)

# Tests
add_subdirectory(tests)
