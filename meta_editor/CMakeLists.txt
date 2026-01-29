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

# Meta Editor SDK library
# Core SDK - always included
add_library(meta_editor
    # Core
    src/core/editor.cpp
    src/canvas.cpp
    src/selection_manager.cpp
    src/tool_manager.cpp
    src/command.cpp
    src/exporter.cpp
    src/serializer.cpp
    src/snap_helper.cpp
    src/connector.cpp
    src/svg_importer.cpp
    # Built-in tools
    src/tools/select_tool.cpp
    src/tools/pen_tool.cpp
    src/tools/shape_tool.cpp
    src/tools/text_tool.cpp
    src/tools/line_tool.cpp
    src/tools/connector_tool.cpp
    src/tools/freehand_tool.cpp
    src/tools/sticky_note_tool.cpp
    src/tools/hand_tool.cpp
    src/tools/eraser_tool.cpp
    src/tools/frame_tool.cpp
    src/tools/image_tool.cpp
    src/tools/laser_tool.cpp
    # Built-in panels (optional - users can use or ignore)
    src/view/context_toolbar.cpp
    src/view/zoom_panel.cpp
    src/view/navigator_panel.cpp
    src/view/shape_collection_panel.cpp
    src/view/tool_panel.cpp
    src/view/properties_panel.cpp
    src/view/layers_panel.cpp
    src/view/align_panel.cpp
    src/view/shortcut_overlay.cpp
    src/view/line_style_panel.cpp
    src/view/export_panel.cpp
    src/view/template_panel.cpp
    src/view/shortcut_panel.cpp
    src/view/context_menu.cpp
    src/view/history_panel.cpp
    src/view/widget_panel.cpp
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
set_target_properties(meta_editor PROPERTIES FOLDER sdk/)

# Demo executable (full app with all panels - reference implementation)
add_executable(meta_editor_demo
    examples/demo.cpp
    examples/full_editor_app.cpp
)

target_include_directories(meta_editor_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)

target_link_libraries(meta_editor_demo
    PRIVATE
        meta_editor
)
set_target_properties(meta_editor_demo PROPERTIES FOLDER sdk/)
# SDK minimal example (no UI panels)
add_executable(sdk_minimal_demo
    examples/sdk_minimal.cpp
)

target_link_libraries(sdk_minimal_demo
    PRIVATE
        meta_editor
)

# Line Tool demo (custom tool + panel example)
add_executable(line_tool_demo
    examples/line_tool_demo.cpp
)

target_link_libraries(line_tool_demo
    PRIVATE
        meta_editor
)
set_target_properties(line_tool_demo PROPERTIES FOLDER sdk/)
# SVG Editor demo (Inkscape-like)
add_executable(svg_editor_demo
    examples/svg_editor_demo.cpp
)

target_link_libraries(svg_editor_demo
    PRIVATE
        meta_editor
)
set_target_properties(svg_editor_demo PROPERTIES FOLDER sdk/)
# Copy assets to build directory
add_custom_command(TARGET meta_editor_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:meta_editor_demo>/assets
    COMMENT "Copying assets to build directory"
)
