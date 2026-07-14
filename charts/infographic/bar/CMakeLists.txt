add_library(infographic_layout_bar STATIC
    src/bar_layout_renderer.cpp
)

target_link_libraries(infographic_layout_bar PUBLIC infographic_core)

set_target_properties(infographic_layout_bar PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
