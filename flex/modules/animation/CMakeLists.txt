# Flex target-independent animation module

add_library(
  flex_animation STATIC
  numeric_expression.cpp
  playback.cpp
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/animation.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/animation/keyframe.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/animation/numeric_expression.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/animation/playback.h)

target_include_directories(
  flex_animation
  PUBLIC $<BUILD_INTERFACE:${FLEX_ENGINE_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
target_compile_features(flex_animation PUBLIC cxx_std_17)
target_link_libraries(flex_animation PUBLIC Flex::Expression)
set_target_properties(
  flex_animation PROPERTIES FOLDER "flex/animation"
                            POSITION_INDEPENDENT_CODE ON)

add_library(Flex::Animation ALIAS flex_animation)
