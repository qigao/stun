# Flex scene, animation playback, physics, and state runtime

file(GLOB FLEX_RUNTIME_SOURCES CONFIGURE_DEPENDS *.c*)
file(GLOB_RECURSE FLEX_RUNTIME_HEADERS CONFIGURE_DEPENDS
     ${FLEX_ENGINE_INCLUDE_DIR}/flex/core/*.h*
     ${FLEX_ENGINE_INCLUDE_DIR}/flex/runtime/*.h*)

add_library(flex_runtime STATIC ${FLEX_RUNTIME_SOURCES} ${FLEX_RUNTIME_HEADERS})

target_include_directories(
  flex_runtime
  PUBLIC $<BUILD_INTERFACE:${FLEX_ENGINE_INCLUDE_DIR}>
         $<BUILD_INTERFACE:${FLEX_ENGINE_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR}
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/mir>
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/miniblas>)
target_compile_features(flex_runtime PUBLIC cxx_std_17)

find_package(box2d CONFIG REQUIRED)
target_link_libraries(
  flex_runtime
  PUBLIC Flex::Render Flex::Animation Flex::Expression TurboUtils::Core
  PRIVATE box2d::box2d miniblas_linalg)

if(WIN32)
  target_compile_definitions(flex_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(flex_runtime PUBLIC /bigobj)
endif()
set_target_properties(
  flex_runtime PROPERTIES FOLDER "flex/runtime" POSITION_INDEPENDENT_CODE ON)

add_library(flex_core INTERFACE)
target_link_libraries(flex_core INTERFACE flex_runtime)
set_target_properties(flex_core PROPERTIES FOLDER "flex/runtime")

add_library(Flex::Runtime ALIAS flex_runtime)
add_library(Flex::Core ALIAS flex_core)
