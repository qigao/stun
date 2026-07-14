add_library(infographic_layout_swot STATIC
    src/swot_layout_renderer.cpp
)

target_link_libraries(infographic_layout_swot PUBLIC infographic_core)

set_target_properties(infographic_layout_swot PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
