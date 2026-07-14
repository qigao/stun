# Flex Engine - Visual Core with DSL Parser
cmake_minimum_required(VERSION 3.16)

project(flex LANGUAGES C CXX)
find_package(Stb REQUIRED)

# ============================================================================
# Parser Generation (re2c + lemon)
# ============================================================================

if(NOT RE2C_EXECUTABLE)
  find_program(RE2C_EXECUTABLE re2c)
endif()
if(NOT RE2C_EXECUTABLE)
  message(FATAL_ERROR "Flex: re2c not found - required for DSL parser")
endif()

if(NOT LEMON_EXECUTABLE)
  find_program(LEMON_EXECUTABLE lemon)
endif()
if(NOT LEMON_EXECUTABLE)
  message(FATAL_ERROR "Flex: lemon not found - required for DSL parser")
endif()

if(NOT LEMPAR)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tools/lemon/lempar.c")
    set(LEMPAR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/lemon/lempar.c")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/tools/lemon/lempar.c")
    set(LEMPAR "${CMAKE_SOURCE_DIR}/tools/lemon/lempar.c")
  else()
    message(FATAL_ERROR "Flex: lempar.c not found - required for DSL parser")
  endif()
endif()

# Generate parser from lemon FIRST (lexer needs the generated token header)
set(FLEX_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/dsl/flex_parser.y)
set(FLEX_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.c)
set(FLEX_PARSER_GEN ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.cpp)
set(FLEX_PARSER_H ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.h)

add_custom_command(
  OUTPUT ${FLEX_PARSER_GEN} ${FLEX_PARSER_H}
  COMMAND ${CMAKE_COMMAND} -E copy ${FLEX_PARSER_Y}
          ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.y
  COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR}
          ${CMAKE_CURRENT_BINARY_DIR}/flex_parser_gen.y
  COMMAND ${CMAKE_COMMAND} -E rename ${FLEX_PARSER_GEN_C} ${FLEX_PARSER_GEN}
  DEPENDS ${FLEX_PARSER_Y} ${LEMON_DEPENDS}
  COMMENT "Generating Flex DSL parser with lemon (template: ${LEMPAR})"
  VERBATIM)

# Generate lexer from re2c (depends on parser header for token values)
set(FLEX_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/dsl/flex_lexer.re)
set(FLEX_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/flex_lexer_gen.cpp)

add_custom_command(
  OUTPUT ${FLEX_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${FLEX_LEXER_GEN} ${FLEX_LEXER_RE}
  DEPENDS ${FLEX_LEXER_RE} ${FLEX_PARSER_H}
  COMMENT "Generating Flex DSL lexer with re2c"
  VERBATIM)

# ============================================================================
# DSL Frontend Library (legacy target name: flex_compiler)
# ============================================================================

add_library(
  flex_compiler STATIC
  ${FLEX_LEXER_GEN} ${FLEX_PARSER_H} ${FLEX_PARSER_GEN} ${FLEX_LEXER_RE}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsl/flex_parser_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/dsl/flex_ast.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/dsl/flex_defs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/dsl/flex_parser.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/dsl/flex_token.h)

target_include_directories(
  flex_compiler
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/ $<INSTALL_INTERFACE:include>)

target_compile_features(flex_compiler PUBLIC cxx_std_17)

set_target_properties(flex_compiler PROPERTIES FOLDER "flex"
                                               POSITION_INDEPENDENT_CODE ON)
target_link_libraries(flex_compiler PUBLIC TurboUtils::Core)

add_library(flex_dsl INTERFACE)
target_link_libraries(flex_dsl INTERFACE flex_compiler)
set_target_properties(flex_dsl PROPERTIES FOLDER "flex")

# ============================================================================
# Shared Numeric Expression Library
# ============================================================================

add_library(
  flex_expression STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/expr_mir.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/core/expr_mir.h)
target_include_directories(
  flex_expression
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/mir>)
target_compile_features(flex_expression PUBLIC cxx_std_17)
target_link_libraries(flex_expression PRIVATE mir_static)
set_target_properties(flex_expression PROPERTIES FOLDER "flex"
                                                 POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Target-independent Animation Library
# ============================================================================

add_library(
  flex_animation STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/animation/numeric_expression.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/animation/playback.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/animation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/animation/numeric_expression.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/animation/playback.h)
target_include_directories(
  flex_animation
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)
target_compile_features(flex_animation PUBLIC cxx_std_17)
target_link_libraries(flex_animation PUBLIC flex_expression)
set_target_properties(flex_animation PROPERTIES FOLDER "flex"
                                                POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Backend-neutral Render Library
# ============================================================================

add_library(
  flex_render STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/renderer_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/render.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/core/renderer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/bridge/renderer.h)
target_include_directories(
  flex_render
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)
target_compile_features(flex_render PUBLIC cxx_std_17)
set_target_properties(flex_render PROPERTIES FOLDER "flex"
                                             POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Core Runtime Library (legacy target name: flex_runtime)
# ============================================================================

file(GLOB_RECURSE FLEX_RUNTIME_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.c*)
list(REMOVE_ITEM FLEX_RUNTIME_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/core/expr_mir.cpp)
file(GLOB_RECURSE FLEX_RUNTIME_HEADERS
     ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/core/*.h*
     ${CMAKE_CURRENT_SOURCE_DIR}/include/flex/runtime/*.h*)
list(APPEND FLEX_RUNTIME_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/core/expr_value.cpp)

add_library(flex_runtime STATIC ${FLEX_RUNTIME_SOURCES} ${FLEX_RUNTIME_HEADERS})

target_include_directories(
  flex_runtime
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/ $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR}
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/mir>
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/miniblas>)

target_compile_features(flex_runtime PUBLIC cxx_std_17)

# Link physics engine (Box2D)
find_package(box2d CONFIG REQUIRED)

target_link_libraries(
  flex_runtime
  PUBLIC flex_animation flex_expression TurboUtils::Core
  PRIVATE box2d::box2d miniblas_linalg)

target_link_libraries(flex_compiler PUBLIC flex_runtime)
# Windows: disable min/max macros that conflict with std::min/std::max
if(WIN32)
    target_compile_definitions(flex_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN )
    target_compile_options(flex_runtime PUBLIC /bigobj)
endif()
set_target_properties(flex_runtime PROPERTIES FOLDER "flex"
                                              POSITION_INDEPENDENT_CODE ON)

add_library(flex_core INTERFACE)
target_link_libraries(flex_core INTERFACE flex_runtime)
set_target_properties(flex_core PROPERTIES FOLDER "flex")

# ============================================================================
# Main Flex Library (facade + lowering + backend integration)
# ============================================================================

add_library(
  flex STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flex.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/asset_resolver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/scene_clone.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lowering/ast_to_runtime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lowering/instance_node_runtime.cpp)

# Windows: disable min/max macros that conflict with std::min/std::max
if(WIN32)
    target_compile_definitions(flex PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN )
    target_compile_options(flex PUBLIC /bigobj)
endif()

target_link_libraries(flex PUBLIC flex_runtime flex_compiler flex_render)

target_include_directories(
  flex
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR})

target_compile_features(flex PUBLIC cxx_std_17)

set_target_properties(flex PROPERTIES FOLDER "flex" POSITION_INDEPENDENT_CODE
                                                    ON)

# ============================================================================
# Optional Renderer Backend Libraries
# ============================================================================

option(FLEX_BUILD_BACKEND_THORVG "Build ThorVG renderer backend" ON)
option(FLEX_BUILD_BACKEND_TUI "Build TUI renderer backend" ON)
option(FLEX_BUILD_BACKEND_NANOVG "Build NanoVG renderer backend" ON)
option(FLEX_BUILD_BACKEND_D2D "Build Direct2D renderer backend" ON)

if(FLEX_BUILD_BACKEND_THORVG)
  add_library(
    flex_backend_thorvg STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/renderer_thorvg.cpp)
  target_link_libraries(flex_backend_thorvg PUBLIC flex_render thorvg TurboUtils::Core)
  target_include_directories(flex_backend_thorvg PRIVATE ${Stb_INCLUDE_DIR})
  target_compile_features(flex_backend_thorvg PUBLIC cxx_std_17)
  set_target_properties(flex_backend_thorvg PROPERTIES FOLDER "flex/backends"
                                                        POSITION_INDEPENDENT_CODE ON)
endif()

if(FLEX_BUILD_BACKEND_TUI)
  add_library(
    flex_backend_tui STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/renderer_tui.cpp)
  target_link_libraries(flex_backend_tui PUBLIC flex_render tango)
  target_compile_features(flex_backend_tui PUBLIC cxx_std_17)
  set_target_properties(flex_backend_tui PROPERTIES FOLDER "flex/backends"
                                                     POSITION_INDEPENDENT_CODE ON)
endif()

if(FLEX_BUILD_BACKEND_NANOVG)
  find_package(OpenGL REQUIRED)
  find_package(glad CONFIG REQUIRED)
  find_package(lunasvg CONFIG REQUIRED)
  find_path(FLEX_LUNASVG_INCLUDE_ROOT NAMES lunasvg/lunasvg.h REQUIRED)
  set(FLEX_OPENGL_LIB)
  if(TARGET OpenGL::GL)
    list(APPEND FLEX_OPENGL_LIB OpenGL::GL)
  elseif(TARGET OpenGL::OpenGL)
    list(APPEND FLEX_OPENGL_LIB OpenGL::OpenGL)
  elseif(WIN32)
    list(APPEND FLEX_OPENGL_LIB opengl32)
  endif()

  add_library(
    flex_backend_nanovg STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/renderer_nanovg.cpp)
  target_link_libraries(
    flex_backend_nanovg
    PUBLIC flex_render nanovg glad::glad lunasvg::lunasvg ${FLEX_OPENGL_LIB})
  target_include_directories(flex_backend_nanovg PRIVATE ${FLEX_LUNASVG_INCLUDE_ROOT})
  target_compile_features(flex_backend_nanovg PUBLIC cxx_std_17)
  set_target_properties(flex_backend_nanovg PROPERTIES FOLDER "flex/backends"
                                                        POSITION_INDEPENDENT_CODE ON)
endif()

if(FLEX_BUILD_BACKEND_D2D)
  find_package(lunasvg CONFIG REQUIRED)
  if(NOT FLEX_LUNASVG_INCLUDE_ROOT)
    find_path(FLEX_LUNASVG_INCLUDE_ROOT NAMES lunasvg/lunasvg.h REQUIRED)
  endif()
  add_library(
    flex_backend_d2d STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/renderer_d2d.cpp)
  target_link_libraries(flex_backend_d2d PUBLIC flex_render lunasvg::lunasvg)
  target_include_directories(flex_backend_d2d PRIVATE ${FLEX_LUNASVG_INCLUDE_ROOT})
  if(WIN32)
    target_compile_definitions(flex_backend_d2d PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(flex_backend_d2d PUBLIC d2d1 dwrite windowscodecs ole32)
  endif()
  target_compile_features(flex_backend_d2d PUBLIC cxx_std_17)
  set_target_properties(flex_backend_d2d PROPERTIES FOLDER "flex/backends"
                                                     POSITION_INDEPENDENT_CODE ON)
endif()

# ============================================================================
# Binary Format Library (optional)
# ============================================================================

option(FLEX_BUILD_BINARY "Build binary format library" ON)
if(FLEX_BUILD_BINARY)
  add_subdirectory(binary)
  message(STATUS "Flex: Binary format library enabled")
endif()

# ============================================================================
# Compiler Tool (optional)
# ============================================================================

option(FLEX_BUILD_TOOLS "Build compiler tools" ON)
if(FLEX_BUILD_TOOLS AND FLEX_BUILD_BINARY)
  add_subdirectory(tools)
  message(STATUS "Flex: Compiler tools enabled")
endif()

# ============================================================================
# Examples & Tests
# ============================================================================

add_subdirectory(examples)
add_subdirectory(tests)

# ============================================================================
# Install
# ============================================================================

set(FLEX_EXPORT_TARGETS
  flex
  flex_expression
  flex_animation
  flex_render
  flex_compiler
  flex_runtime
  flex_dsl
  flex_core)

install(
  TARGETS ${FLEX_EXPORT_TARGETS}
  EXPORT flex-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(
  DIRECTORY include/flex
  DESTINATION include
  PATTERN "bridge/renderer_*.h" EXCLUDE)
install(FILES include/flex.h DESTINATION include)
