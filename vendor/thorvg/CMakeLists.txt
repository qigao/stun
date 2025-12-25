cmake_minimum_required(VERSION 3.15)
project(
  thorvg
  VERSION 1.0.0
  LANGUAGES CXX)

# Enable folder organization in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ============================================================
# Build Options
# ============================================================
option(THORVG_BUILD_SHARED "Build shared library" ON)
option(THORVG_ENGINE_SW "Enable SW raster engine" ON)
option(THORVG_ENGINE_GL "Enable GL raster engine" ON)
option(THORVG_ENGINE_WG "Enable WG raster engine" OFF)
option(THORVG_LOADER_SVG "Enable SVG loader" ON)
option(THORVG_LOADER_PNG "Enable PNG loader" ON)
option(THORVG_LOADER_JPG "Enable JPG loader" OFF)
option(THORVG_LOADER_WEBP "Enable WEBP loader" OFF)
option(THORVG_LOADER_LOTTIE "Enable Lottie loader" OFF)
option(THORVG_LOADER_TTF "Enable TTF loader" ON)
option(THORVG_SAVER_GIF "Enable GIF saver" OFF)
option(THORVG_THREADS "Enable multi-threading" ON)
option(THORVG_SIMD "Enable SIMD (AVX/NEON)" ON)
option(THORVG_LOG "Enable log messages" OFF)
option(THORVG_FILE_IO "Enable file I/O" ON)
# ============================================================
# OpenGL Variant
# ============================================================
if(THORVG_ENGINE_GL)
  option(THORVG_GL_USE_GLES "Use OpenGL ES instead of OpenGL" OFF)
endif()

# ============================================================
# Compiler Settings
# ============================================================
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Generate config.h
# ============================================================
set(THORVG_VERSION_STRING "${PROJECT_VERSION}")

if(THORVG_THREADS)
  set(THORVG_THREAD_SUPPORT 1)
endif()

if(THORVG_ENGINE_SW)
  set(THORVG_SW_RASTER_SUPPORT 1)
endif()

if(THORVG_ENGINE_GL)
  set(THORVG_GL_RASTER_SUPPORT 1)
  if(THORVG_GL_USE_GLES)
    set(THORVG_GL_TARGET_GLES 1)
  else()
    set(THORVG_GL_TARGET_GL 1)
  endif()
endif()

if(THORVG_ENGINE_WG)
  set(THORVG_WG_RASTER_SUPPORT 1)
endif()

if(THORVG_PARTIAL_RENDER)
  set(THORVG_PARTIAL_RENDER_SUPPORT 1)
endif()

if(THORVG_LOADER_SVG)
  set(THORVG_SVG_LOADER_SUPPORT 1)
endif()

if(THORVG_LOADER_PNG)
  set(THORVG_PNG_LOADER_SUPPORT 1)
endif()

if(THORVG_LOADER_JPG)
  set(THORVG_JPG_LOADER_SUPPORT 1)
endif()

if(THORVG_LOADER_WEBP)
  set(THORVG_WEBP_LOADER_SUPPORT 1)
endif()

if(THORVG_LOADER_LOTTIE)
  set(THORVG_LOTTIE_LOADER_SUPPORT 1)
endif()

if(THORVG_LOADER_TTF)
  set(THORVG_TTF_LOADER_SUPPORT 1)
endif()

if(THORVG_SAVER_GIF)
  set(THORVG_GIF_SAVER_SUPPORT 1)
endif()

if(THORVG_LOG)
  set(THORVG_LOG_ENABLED 1)
endif()

if(THORVG_FILE_IO)
  set(THORVG_FILE_IO_SUPPORT 1)
endif()

if(THORVG_SIMD)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    set(THORVG_AVX_VECTOR_SUPPORT 1)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM")
    set(THORVG_NEON_VECTOR_SUPPORT 1)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AARCH64")
    set(THORVG_NEON_VECTOR_SUPPORT 1)
  endif()
endif()

set(WIN32_LEAN_AND_MEAN 1)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/config.h")

# ============================================================
# Collect Source Files
# ============================================================

# Common utilities
set(COMMON_UTILS_SOURCES src/common/tvgColor.cpp src/common/tvgCompressor.cpp
                         src/common/tvgMath.cpp src/common/tvgStr.cpp)

# Core renderer
set(RENDERER_SOURCES
    src/renderer/tvgAccessor.cpp
    src/renderer/tvgAnimation.cpp
    src/renderer/tvgCanvas.cpp
    src/renderer/tvgFill.cpp
    src/renderer/tvgInitializer.cpp
    src/renderer/tvgLoader.cpp
    src/renderer/tvgPaint.cpp
    src/renderer/tvgPicture.cpp
    src/renderer/tvgRender.cpp
    src/renderer/tvgSaver.cpp
    src/renderer/tvgScene.cpp
    src/renderer/tvgShape.cpp
    src/renderer/tvgTaskScheduler.cpp
    src/renderer/tvgText.cpp)

# SW Engine
set(SW_ENGINE_SOURCES)
if(THORVG_ENGINE_SW)
  set(SW_ENGINE_SOURCES
      src/renderer/sw_engine/tvgSwFill.cpp
      src/renderer/sw_engine/tvgSwImage.cpp
      src/renderer/sw_engine/tvgSwMath.cpp
      src/renderer/sw_engine/tvgSwMemPool.cpp
      src/renderer/sw_engine/tvgSwPostEffect.cpp
      src/renderer/sw_engine/tvgSwRaster.cpp
      src/renderer/sw_engine/tvgSwRenderer.cpp
      src/renderer/sw_engine/tvgSwRle.cpp
      src/renderer/sw_engine/tvgSwShape.cpp
      src/renderer/sw_engine/tvgSwStroke.cpp)
endif()

# GL Engine
set(GL_ENGINE_SOURCES)
if(THORVG_ENGINE_GL)
  set(GL_ENGINE_SOURCES
      src/renderer/gl_engine/tvgGl.cpp
      src/renderer/gl_engine/tvgGlEffect.cpp
      src/renderer/gl_engine/tvgGlGeometry.cpp
      src/renderer/gl_engine/tvgGlGpuBuffer.cpp
      src/renderer/gl_engine/tvgGlProgram.cpp
      src/renderer/gl_engine/tvgGlRenderer.cpp
      src/renderer/gl_engine/tvgGlRenderPass.cpp
      src/renderer/gl_engine/tvgGlRenderTarget.cpp
      src/renderer/gl_engine/tvgGlRenderTask.cpp
      src/renderer/gl_engine/tvgGlShader.cpp
      src/renderer/gl_engine/tvgGlShaderSrc.cpp
      src/renderer/gl_engine/tvgGlTessellator.cpp)
endif()

# Raw loader (always enabled)
set(LOADER_SOURCES src/loaders/raw/tvgRawLoader.cpp)

# SVG loader
if(THORVG_LOADER_SVG)
  list(
    APPEND
    LOADER_SOURCES
    src/loaders/svg/tvgSvgCssStyle.cpp
    src/loaders/svg/tvgSvgLoader.cpp
    src/loaders/svg/tvgSvgPath.cpp
    src/loaders/svg/tvgSvgSceneBuilder.cpp
    src/loaders/svg/tvgSvgUtil.cpp
    src/loaders/svg/tvgXmlParser.cpp)
endif()

# PNG loader (using built-in LodePNG, no external dependency)
if(THORVG_LOADER_PNG)
  list(APPEND LOADER_SOURCES src/loaders/png/tvgLodePng.cpp
       src/loaders/png/tvgPngLoader.cpp)
endif()

# TTF loader (built-in, no external dependency)
if(THORVG_LOADER_TTF)
  list(APPEND LOADER_SOURCES src/loaders/ttf/tvgTtfLoader.cpp
       src/loaders/ttf/tvgTtfReader.cpp)
endif()

# Saver sources
set(SAVER_SOURCES)

# Combine all sources
set(THORVG_SOURCES
    ${COMMON_UTILS_SOURCES} ${RENDERER_SOURCES} ${SW_ENGINE_SOURCES}
    ${GL_ENGINE_SOURCES} ${LOADER_SOURCES} ${SAVER_SOURCES})

# ============================================================
# Create Library
# ============================================================
if(THORVG_BUILD_SHARED)
  add_library(thorvg SHARED ${THORVG_SOURCES})
  target_compile_definitions(thorvg PRIVATE TVG_EXPORT TVG_BUILD)
else()
  add_library(thorvg STATIC ${THORVG_SOURCES})
  target_compile_definitions(thorvg PUBLIC TVG_STATIC)
endif()

# Include directories
target_include_directories(
  thorvg
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/src/common
          ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
          ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders
          ${CMAKE_CURRENT_SOURCE_DIR}/src/savers
          ${CMAKE_CURRENT_BINARY_DIR})

# Add engine-specific includes
if(THORVG_ENGINE_SW)
  target_include_directories(
    thorvg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/sw_engine)
endif()

if(THORVG_ENGINE_GL)
  target_include_directories(
    thorvg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gl_engine)
endif()

# Add loader-specific includes
if(THORVG_LOADER_SVG)
  target_include_directories(
    thorvg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/svg)
endif()

if(THORVG_LOADER_PNG)
  target_include_directories(
    thorvg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/png)
endif()

if(THORVG_LOADER_TTF)
  target_include_directories(
    thorvg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/ttf)
endif()

target_include_directories(thorvg
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/raw)

# ============================================================
# Compiler Flags
# ============================================================
if(MSVC)
  target_compile_definitions(thorvg PRIVATE NOMINMAX)
  target_compile_options(thorvg PRIVATE /W3)
  if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(thorvg PRIVATE /O2)
  endif()
else()
  target_compile_options(thorvg PRIVATE -Wno-unknown-pragmas
                                        $<$<NOT:$<CONFIG:Debug>>:-O3>)

  if(NOT THORVG_LOG)
    target_compile_options(thorvg PRIVATE -Wdouble-promotion)
  endif()

  # SIMD flags
  if(THORVG_SIMD)
    if(THORVG_AVX_VECTOR_SUPPORT)
      target_compile_options(thorvg PRIVATE -mavx)
    elseif(THORVG_NEON_VECTOR_SUPPORT)
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        target_compile_options(thorvg PRIVATE -mfpu=neon)
      endif()
    endif()
  endif()

  # Optimization flags
  if(NOT CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT SANITIZE)
    target_compile_options(
      thorvg
      PRIVATE -fno-exceptions
              -fno-rtti
              -fno-stack-protector
              -fno-math-errno
              -fno-unwind-tables
              -fno-asynchronous-unwind-tables
              -Woverloaded-virtual)
  endif()
endif()

# ============================================================
# Dependencies
# ============================================================

# Threading
if(THORVG_THREADS)
  if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(thorvg PRIVATE Threads::Threads)

    # Check for OpenMP (optional for SW engine)
    if(THORVG_ENGINE_SW)
      find_package(OpenMP)
      if(OpenMP_CXX_FOUND)
        target_link_libraries(thorvg PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(thorvg PRIVATE THORVG_SW_OPENMP_SUPPORT=1)
      endif()
    endif()
  endif()
endif()

# Symbol visibility
set_target_properties(
  thorvg
  PROPERTIES CXX_VISIBILITY_PRESET hidden
   FOLDER vendor/thorvg
             VISIBILITY_INLINES_HIDDEN ON
             VERSION ${PROJECT_VERSION}
             SOVERSION 1)

# ============================================================
# Examples and Tests
# ============================================================

add_subdirectory(examples)

add_subdirectory(test)

# ============================================================
# Installation
# ============================================================
include(GNUInstallDirs)

install(
  TARGETS thorvg
  EXPORT thorvgTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES inc/thorvg.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ============================================================
# Build Summary
# ============================================================
message(STATUS "========================================")
message(STATUS "ThorVG Configuration")
message(STATUS "========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Library Type: ${THORVG_BUILD_SHARED}")
message(STATUS "----------------------------------------")
message(STATUS "Engines:")
message(STATUS "  SW Raster: ${THORVG_ENGINE_SW}")
message(STATUS "  GL Raster: ${THORVG_ENGINE_GL}")
if(THORVG_ENGINE_GL)
  message(STATUS "    GL Variant: ${THORVG_GL_USE_GLES}")
endif()
message(STATUS "  WG Raster: ${THORVG_ENGINE_WG}")
message(STATUS "----------------------------------------")
message(STATUS "Loaders:")
message(STATUS "  SVG: ${THORVG_LOADER_SVG}")
message(STATUS "  PNG: ${THORVG_LOADER_PNG}")
message(STATUS "  JPG: ${THORVG_LOADER_JPG}")
message(STATUS "  WEBP: ${THORVG_LOADER_WEBP}")
message(STATUS "  Lottie: ${THORVG_LOADER_LOTTIE}")
message(STATUS "  TTF: ${THORVG_LOADER_TTF}")
message(STATUS "----------------------------------------")
message(STATUS "Features:")
message(STATUS "  Multi-Threading: ${THORVG_THREADS}")
message(STATUS "  SIMD: ${THORVG_SIMD}")
message(STATUS "  Partial Rendering: ${THORVG_PARTIAL_RENDER}")
message(STATUS "  File I/O: ${THORVG_FILE_IO}")
message(STATUS "  Logging: ${THORVG_LOG}")
message(STATUS "----------------------------------------")
message(STATUS "Build Options:")
message(STATUS "  Examples: ${THORVG_BUILD_EXAMPLES}")
message(STATUS "  Tests: ${THORVG_BUILD_TESTS}")
message(STATUS "========================================")
