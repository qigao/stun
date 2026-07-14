add_library(infographic_layout_pie STATIC
    src/pie_layout_renderer.cpp
)

target_link_libraries(infographic_layout_pie PUBLIC infographic_core)

set_target_properties(infographic_layout_pie PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
