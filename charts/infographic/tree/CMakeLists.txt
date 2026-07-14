add_library(infographic_layout_tree STATIC
    src/tree_layout_renderer.cpp
)

target_link_libraries(infographic_layout_tree PUBLIC infographic_core)

set_target_properties(infographic_layout_tree PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
