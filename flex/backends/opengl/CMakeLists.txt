find_package(OpenGL REQUIRED)

add_library(flex_backend_opengl STATIC backend.cpp)
target_link_libraries(flex_backend_opengl PUBLIC flex_render)
if(TARGET OpenGL::GL)
  target_link_libraries(flex_backend_opengl PUBLIC OpenGL::GL)
elseif(TARGET OpenGL::OpenGL)
  target_link_libraries(flex_backend_opengl PUBLIC OpenGL::OpenGL)
elseif(WIN32)
  target_link_libraries(flex_backend_opengl PUBLIC opengl32)
endif()
if(FLEX_OPENGL_RENDER_ENGINE STREQUAL "gcanvas")
  target_link_libraries(flex_backend_opengl PRIVATE flex_render_engine_gcanvas)
  target_compile_definitions(flex_backend_opengl PRIVATE FLEX_OPENGL_ENGINE_GCANVAS=1)
elseif(FLEX_OPENGL_RENDER_ENGINE STREQUAL "nanovg")
  target_link_libraries(flex_backend_opengl PRIVATE flex_render_engine_nanovg)
else()
  message(FATAL_ERROR "Unknown FLEX_OPENGL_RENDER_ENGINE=${FLEX_OPENGL_RENDER_ENGINE}")
endif()
target_include_directories(
  flex_backend_opengl
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(flex_backend_opengl PUBLIC cxx_std_17)
set_target_properties(
  flex_backend_opengl
  PROPERTIES FOLDER "flex/backends" POSITION_INDEPENDENT_CODE ON)
