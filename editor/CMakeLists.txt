# Flex Editor - Standalone Vector Graphics Editor
# Based on flex engine with ThorVG rendering

# Find SDL2
find_package(SDL2 CONFIG REQUIRED)
find_package(clipper2 CONFIG REQUIRED)

# Editor sources
set(EDITOR_SOURCES
    src/main.cpp

    # Core
    src/core/transform.cpp

    # Model
    src/model/node.cpp
    src/model/layer.cpp
    src/model/document.cpp

    # Command
    src/command/boolean_commands.cpp

    # ViewModel
    src/viewmodel/selection.cpp
    src/viewmodel/editor_vm.cpp

    # Input
    src/input/shortcut.cpp

    # Tools
    src/tool/select_tool.cpp
    src/tool/pan_tool.cpp
    src/tool/draw_tool.cpp
    src/tool/pen_tool.cpp
    src/tool/text_tool.cpp
    src/tool/snap_helper.cpp
    src/tool/path_editor.cpp

    # View
    src/view/panel.cpp
    src/view/color_picker.cpp
    src/view/gradient_picker.cpp
    src/view/ruler_guide.cpp
    src/view/effects_panel.cpp
    src/view/layer_panel.cpp
    src/view/toolbar.cpp

    src/view/context_menu.cpp
    src/view/history_panel.cpp
    src/view/align_panel.cpp
    src/view/zoom_panel.cpp
    src/view/navigator_panel.cpp
    src/view/grid.cpp
)

# Create executable
add_executable(flex_editor ${EDITOR_SOURCES})

# Link dependencies
target_link_libraries(flex_editor PRIVATE
    flex
    tvgbox2
    SDL2::SDL2
    SDL2::SDL2main
    Clipper2::Clipper2
)

# Include directories
target_include_directories(flex_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tvgbox2/include
)

# Set folder for IDE
set_target_properties(flex_editor PROPERTIES
    FOLDER "editor"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# Copy resources to output directory
# add_custom_command(TARGET flex_editor POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_CURRENT_SOURCE_DIR}/resources
#     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources
# )
