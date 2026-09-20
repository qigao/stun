# Flex numeric expression module

add_library(
  flex_expression STATIC
  expr_mir.cpp
  expr_c.cpp
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/core/expr_mir.h
  ${FLEX_ENGINE_INCLUDE_DIR}/flex/core/expr_c.h)

target_include_directories(
  flex_expression
  PUBLIC $<BUILD_INTERFACE:${FLEX_ENGINE_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>
  PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/mir>)
target_compile_features(flex_expression PUBLIC cxx_std_17)
target_link_libraries(flex_expression PRIVATE mir_static)
set_target_properties(
  flex_expression PROPERTIES FOLDER "flex/expression"
                             POSITION_INDEPENDENT_CODE ON)

add_library(Flex::Expression ALIAS flex_expression)
