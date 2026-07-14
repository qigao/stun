add_library(infographic_layout_grid STATIC
    src/grid_layout_renderer.cpp
)

target_link_libraries(infographic_layout_grid PUBLIC infographic_core)

set_target_properties(infographic_layout_grid PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
