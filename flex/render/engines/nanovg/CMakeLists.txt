find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)

set(FLEX_NANOVG_OPENGL_LIB)
if(TARGET OpenGL::GL)
  list(APPEND FLEX_NANOVG_OPENGL_LIB OpenGL::GL)
elseif(TARGET OpenGL::OpenGL)
  list(APPEND FLEX_NANOVG_OPENGL_LIB OpenGL::OpenGL)
elseif(WIN32)
  list(APPEND FLEX_NANOVG_OPENGL_LIB opengl32)
endif()

add_library(flex_render_engine_nanovg STATIC renderer.cpp)
target_link_libraries(
  flex_render_engine_nanovg
  PRIVATE flex_render nanovg glad::glad gCanvas::Svg
          ${FLEX_NANOVG_OPENGL_LIB})
target_include_directories(
  flex_render_engine_nanovg
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../backends/opengl/include)
target_compile_features(flex_render_engine_nanovg PUBLIC cxx_std_17)
set_target_properties(
  flex_render_engine_nanovg
  PROPERTIES FOLDER "flex/render/engines" POSITION_INDEPENDENT_CODE ON)
