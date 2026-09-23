cmake_minimum_required(VERSION 3.22)

project(gcanvas_android_lifecycle_test LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# This APK embeds gCanvas and FreeType from source and never installs them.
# Skip install/export generation so fetched test dependencies do not need to
# participate in gCanvas's installed package export set.
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HVF ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG VER-2-14-3
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(freetype)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(STB_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/stb" CACHE PATH "" FORCE)

set(GCANVAS_BUILD_OPENGL OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_GLES ON CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_ANDROID_EGL ON CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_SVG OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_WINDOW OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_GPU_TESTS OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

get_filename_component(
  GCANVAS_SOURCE_DIR
  "${CMAKE_CURRENT_LIST_DIR}/../../../../../.."
  ABSOLUTE)
add_subdirectory("${GCANVAS_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/gcanvas")

add_library(gcanvas_android_lifecycle_test SHARED lifecycle_jni.cpp)
target_compile_features(gcanvas_android_lifecycle_test PRIVATE cxx_std_17)
target_link_libraries(gcanvas_android_lifecycle_test
  PRIVATE
    gCanvas::AndroidEGL
    android
    log)
