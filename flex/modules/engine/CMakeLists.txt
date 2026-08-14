# Flex Engine facade and AST-to-runtime lowering

add_library(
  flex STATIC
  flex.cpp
  runtime/asset_resolver.cpp
  runtime/scene_clone.cpp
  lowering/ast_to_runtime.cpp
  lowering/semantic_validator.cpp
  lowering/instance_node_runtime.cpp)

if(WIN32)
  target_compile_definitions(flex PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(flex PUBLIC /bigobj)
endif()

target_link_libraries(flex PUBLIC Flex::Runtime Flex::Compiler Flex::Render)
target_include_directories(
  flex
  PUBLIC $<BUILD_INTERFACE:${FLEX_ENGINE_INCLUDE_DIR}>
         $<BUILD_INTERFACE:${FLEX_ENGINE_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR}
  PRIVATE ${FLEX_ENGINE_ROOT}/modules/runtime)
target_compile_features(flex PUBLIC cxx_std_17)
set_target_properties(
  flex PROPERTIES FOLDER "flex/engine" POSITION_INDEPENDENT_CODE ON)

add_library(Flex::Engine ALIAS flex)
