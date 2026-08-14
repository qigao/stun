add_library(flex_render_engine_gcanvas STATIC renderer.cpp)
target_link_libraries(
  flex_render_engine_gcanvas PUBLIC flex_render gCanvas::Core gCanvas::Svg)
if(TARGET gCanvas::OpenGL)
  target_link_libraries(flex_render_engine_gcanvas PUBLIC gCanvas::OpenGL)
  target_compile_definitions(flex_render_engine_gcanvas PRIVATE
                             FLEX_GCANVAS_HAS_OPENGL=1)
endif()
target_include_directories(
  flex_render_engine_gcanvas
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(flex_render_engine_gcanvas PUBLIC cxx_std_17)
set_target_properties(
  flex_render_engine_gcanvas
  PROPERTIES FOLDER "flex/render/engines" POSITION_INDEPENDENT_CODE ON)
