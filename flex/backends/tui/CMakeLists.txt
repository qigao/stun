add_library(flex_backend_tui STATIC renderer.cpp)

target_link_libraries(flex_backend_tui PUBLIC flex_render tango)
target_include_directories(
  flex_backend_tui
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(flex_backend_tui PUBLIC cxx_std_17)
set_target_properties(
  flex_backend_tui
  PROPERTIES FOLDER "flex/backends" POSITION_INDEPENDENT_CODE ON)
