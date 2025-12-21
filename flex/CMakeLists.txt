# Flex Engine - Visual Core with DSL Parser
cmake_minimum_required(VERSION 3.16)

project(flex LANGUAGES C CXX)

# ============================================================================
# Library
# ============================================================================

set(FLEX_SOURCES
    src/flex.cpp
    src/types.cpp
    src/node.cpp
    src/group.cpp
    src/shape.cpp
    src/text.cpp
    src/image.cpp
    src/svg.cpp
    src/instance.cpp
    src/solo.cpp
    src/artboard.cpp
    src/renderer_thorvg.cpp
    src/timeline.cpp
    src/machine.cpp
    src/script.cpp
    src/physics.cpp
    src/binding.cpp
    src/path.cpp
    src/component.cpp)
    # Note: Removed ${LEXER_GEN} and ${PARSER_GEN_C} - using hand-written parser now

set(FLEX_HEADERS
    include/flex/flex.h
    include/flex/types.h
    include/flex/geometry.h
    include/flex/node.h
    include/flex/group.h
    include/flex/shape.h
    include/flex/text.h
    include/flex/image.h
    include/flex/svg.h
    include/flex/instance.h
    include/flex/solo.h
    include/flex/artboard.h
    include/flex/renderer.h
    include/flex/timeline.h
    include/flex/machine.h
    include/flex/script.h
    include/flex/physics.h
    include/flex/binding.h
    include/flex/path.h
    include/flex/component.h
    include/flex/dsl_loader.h)

add_library(flex STATIC ${FLEX_SOURCES} ${FLEX_HEADERS})

target_include_directories(
  flex
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
          ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_features(flex PUBLIC cxx_std_17)

# Link rendering backend (ThorVG)
target_link_libraries(flex PUBLIC thorvg)

# Link scripting engine (QuickJS)
target_link_libraries(flex PRIVATE qjs)

# Link physics engine (Box2D)
find_package(box2d CONFIG REQUIRED)
target_link_libraries(flex PRIVATE box2d::box2d)

set_target_properties(flex PROPERTIES FOLDER "flex" POSITION_INDEPENDENT_CODE
                                                    ON)

# ============================================================================
# Examples & Tests
# ============================================================================

add_subdirectory(examples)

option(FLEX_BUILD_TESTS "Build Flex tests" ON)

add_subdirectory(tests)

# ============================================================================
# Install
# ============================================================================

install(
  TARGETS flex
  EXPORT flex-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/flex DESTINATION include)
