add_library(flex_backend_d2d STATIC renderer.cpp)
target_link_libraries(flex_backend_d2d PUBLIC flex_render gCanvas::Svg)
target_include_directories(
  flex_backend_d2d
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
if(WIN32)
  target_compile_definitions(
    flex_backend_d2d PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(
    flex_backend_d2d PUBLIC d2d1 dwrite windowscodecs ole32)
endif()
target_compile_features(flex_backend_d2d PUBLIC cxx_std_17)
set_target_properties(
  flex_backend_d2d
  PROPERTIES FOLDER "flex/backends" POSITION_INDEPENDENT_CODE ON)
