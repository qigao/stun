cmake_minimum_required(VERSION 3.15)

project(nanovg LANGUAGES C)

# Add the nanovg static library
add_library(
  nanovg STATIC
  src/nanovg.c
  src/nanovg.h
  src/fontstash.h
  src/stb_image.h
  src/stb_truetype.h
  src/nanovg_gl.h
  src/nanovg_gl_utils.h)

# Create an alias for consistent naming
add_library(nanovg::nanovg ALIAS nanovg)
set_target_properties(nanovg PROPERTIES FOLDER nanovg/)

# Set C standard
set_target_properties(
  nanovg
  PROPERTIES C_STANDARD 99
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)

# Include directories
target_include_directories(
  nanovg PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                $<INSTALL_INTERFACE:include>)

# Platform-specific definitions
if(WIN32)
  target_compile_definitions(nanovg PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Suppress common warnings in nanovg source
if(MSVC)
  target_compile_options(nanovg PRIVATE /wd4005 /wd4456 /wd4457)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(nanovg PRIVATE -Wno-unused-result)
endif()

# Option to build examples
option(NANOVG_BUILD_EXAMPLES "Build nanovg example applications" OFF)

if(NANOVG_BUILD_EXAMPLES)
  # Find dependencies
  find_package(glfw3 CONFIG REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLEW REQUIRED)

  # Common example sources
  set(EXAMPLE_COMMON_SOURCES example/demo.c example/perf.c)

  # Helper function to create examples
  function(add_nanovg_example TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE} ${EXAMPLE_COMMON_SOURCES})
    target_link_libraries(${TARGET_NAME} PRIVATE nanovg::nanovg glfw OpenGL::GL
                                                 GLEW::GLEW)
    target_include_directories(${TARGET_NAME} PRIVATE example)
    target_compile_definitions(${TARGET_NAME} PRIVATE NANOVG_GLEW)

    if(WIN32)
      target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(MSVC)
      target_compile_options(${TARGET_NAME} PRIVATE /wd4244 /wd4305)
    endif()
  endfunction()

  # Create example targets
  add_nanovg_example(example_gl2 example/example_gl2.c)
  add_nanovg_example(example_gl3 example/example_gl3.c)
  add_nanovg_example(example_fbo example/example_fbo.c)
  add_nanovg_example(example_gles2 example/example_gles2.c)
  add_nanovg_example(example_gles3 example/example_gles3.c)

  # MSAA variants
  add_nanovg_example(example_gl2_msaa example/example_gl2.c)
  target_compile_definitions(example_gl2_msaa PRIVATE DEMO_MSAA)

  add_nanovg_example(example_gl3_msaa example/example_gl3.c)
  target_compile_definitions(example_gl3_msaa PRIVATE DEMO_MSAA)
endif()
