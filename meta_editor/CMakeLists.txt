cmake_minimum_required(VERSION 3.15)
project(meta_editor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(SDL2 CONFIG QUIET)
find_package(OpenGL REQUIRED)
# Find parent flex and flexUI
set(FLEX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../flex)
set(FLEXUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../flexUI)

# Add flex and flexUI as subdirectories if not already added
if(NOT TARGET flex)
    add_subdirectory(${FLEX_DIR} ${CMAKE_BINARY_DIR}/flex)
endif()

if(NOT TARGET flexUI)
    add_subdirectory(${FLEXUI_DIR} ${CMAKE_BINARY_DIR}/flexUI)
endif()

# Find SDL2
find_package(SDL2 REQUIRED)

# Meta Editor library
add_library(meta_editor
    src/canvas.cpp
    src/selection_manager.cpp
    src/tool_manager.cpp
    src/command.cpp
    src/exporter.cpp
    src/tools/select_tool.cpp
    src/tools/pen_tool.cpp
    src/tools/shape_tool.cpp
    src/view/context_toolbar.cpp
    src/view/zoom_panel.cpp
    src/view/navigator_panel.cpp
    src/view/shape_collection_panel.cpp
    src/view/tool_panel.cpp
    src/meta_editor.cpp
)

target_include_directories(meta_editor 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FLEXUI_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(meta_editor 
    PUBLIC 
        flex 
        flexUI
        SDL2::SDL2 SDL2::SDL2main OpenGL::GL
)

# Demo executable
add_executable(meta_editor_demo
    examples/demo.cpp
)

target_link_libraries(meta_editor_demo
    PRIVATE
        meta_editor
)

# Copy assets to build directory
add_custom_command(TARGET meta_editor_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:meta_editor_demo>/assets
    COMMENT "Copying assets to build directory"
)
