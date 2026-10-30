# Tango C11 grid -> native pixel drawing recipe (CPU, headless, no GL SDK).
add_library(flex_tango_gl_frame STATIC cell_presenter.cpp cell_input.cpp)
target_link_libraries(flex_tango_gl_frame PUBLIC flex_render tango)
target_include_directories(flex_tango_gl_frame
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(flex_tango_gl_frame PUBLIC cxx_std_17)
set_target_properties(flex_tango_gl_frame PROPERTIES
  FOLDER "flex/backends" POSITION_INDEPENDENT_CODE ON)

# Only the explicit Tango/OpenGL presenter owns a gCanvas GL renderer.
# This never registers the public TangoOpenGL slot or auto-selects generic GL.
if(FLEX_BUILD_BACKEND_TANGO_OPENGL)
  if(NOT TARGET flex_backend_opengl OR
     NOT TARGET flex_render_engine_gcanvas OR
     NOT FLEX_OPENGL_RENDER_ENGINE STREQUAL "gcanvas")
    message(FATAL_ERROR
      "TangoOpenGL requires an explicitly built gCanvas OpenGL backend")
  endif()
  add_library(flex_backend_tango_opengl STATIC gl_presenter.cpp gl_fence.cpp gl_owner.cpp)
  target_link_libraries(flex_backend_tango_opengl
    PUBLIC flex_tango_gl_frame flex_backend_opengl
    PRIVATE flex_render_engine_gcanvas glad::glad)
  target_include_directories(flex_backend_tango_opengl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(flex_backend_tango_opengl PUBLIC cxx_std_17)
  set_target_properties(flex_backend_tango_opengl PROPERTIES
    FOLDER "flex/backends" POSITION_INDEPENDENT_CODE ON)
endif()
