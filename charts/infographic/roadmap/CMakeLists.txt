add_library(infographic_layout_roadmap STATIC
    src/roadmap_layout_renderer.cpp
)

target_link_libraries(infographic_layout_roadmap PUBLIC infographic_core)

set_target_properties(infographic_layout_roadmap PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
