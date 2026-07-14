add_library(infographic_layout_circular STATIC
    src/circular_layout_renderer.cpp
)

target_link_libraries(infographic_layout_circular PUBLIC infographic_core)

set_target_properties(infographic_layout_circular PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
