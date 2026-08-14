# Flex backend-neutral render contracts and renderer registry

add_library(flex_stb_sprintf OBJECT stb_sprintf.cpp)
target_include_directories(flex_stb_sprintf PRIVATE ${Stb_INCLUDE_DIR})
target_compile_features(flex_stb_sprintf PRIVATE cxx_std_17)
set_target_properties(
  flex_stb_sprintf
  PROPERTIES
    FOLDER "flex/render"
    POSITION_INDEPENDENT_CODE ON
    # stb_sprintf intentionally reads up to three bytes beyond the format
    # string terminator. MSVC ASan cannot express the upstream suppression.
    NANOGUI_DISABLE_ADDRESS_SANITIZER ON)

add_library(
  flex_render STATIC
  renderer_factory.cpp
  $<TARGET_OBJECTS:flex_stb_sprintf>
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/render.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/core/renderer.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/bridge/renderer.h)
target_include_directories(
  flex_render
  PUBLIC $<BUILD_INTERFACE:${FLEX_ENGINE_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${Stb_INCLUDE_DIR})
target_compile_features(flex_render PUBLIC cxx_std_17)
set_target_properties(
  flex_render PROPERTIES FOLDER "flex/render" POSITION_INDEPENDENT_CODE ON)

add_library(Flex::Render ALIAS flex_render)
