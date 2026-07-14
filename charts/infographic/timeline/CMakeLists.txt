add_library(infographic_layout_timeline STATIC
    src/timeline_layout_renderer.cpp
)

target_link_libraries(infographic_layout_timeline PUBLIC infographic_core)

set_target_properties(infographic_layout_timeline PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
