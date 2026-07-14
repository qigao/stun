add_library(infographic_layout_funnel STATIC
    src/funnel_layout_renderer.cpp
)

target_link_libraries(infographic_layout_funnel PUBLIC infographic_core)

set_target_properties(infographic_layout_funnel PROPERTIES FOLDER "Modules/FlexInfographic/Layout")
