add_library(infographic_layout_zigzag STATIC
    src/zigzag_layout_renderer.cpp
)

target_link_libraries(infographic_layout_zigzag PUBLIC infographic_core)

set_target_properties(infographic_layout_zigzag PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
