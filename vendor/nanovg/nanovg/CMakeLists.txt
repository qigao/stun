find_package(OpenGL REQUIRED)
if(NOT TARGET glad::glad)
find_package(glad CONFIG REQUIRED)
endif()
# ============================================================================
# Build NanoVG internally
# ============================================================================

# NanoVG source files (embedded in this module)
set(NANOVG_SOURCES
    nanovg.c
    nanovg.h
    nanovg_rough.c
    nanovg_rough.h
    fontstash.h
    stb_image.h
    stb_truetype.h
    nanovg_gl.h
    nanovg_gl_utils.h)

# Build NanoVG as a static library
add_library(nanovg STATIC ${NANOVG_SOURCES})

# Set C standard for NanoVG
set_target_properties(
  nanovg
  PROPERTIES C_STANDARD 99
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             FOLDER vendor/nanovg/)

# NanoVG include directories
target_include_directories(
  nanovg PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Platform-specific definitions for NanoVG
if(WIN32)
  target_compile_definitions(nanovg PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Suppress common warnings in nanovg source
if(MSVC)
  target_compile_options(nanovg PRIVATE /wd4005 /wd4456 /wd4457)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(nanovg PRIVATE -Wno-unused-result)
endif()
 
