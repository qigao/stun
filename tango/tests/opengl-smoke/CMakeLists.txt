cmake_minimum_required(VERSION 3.20)
project(TangoNativeOpenGLPixelQualification LANGUAGES C CXX)

# Independent production-source consumer: no Flex DSL, XML/CSS/JS, or separate
# faux renderer. Only the actual installed Salts Unicode/Platform providers,
# Tango C11, Flex::Render and gCanvas desktop OpenGL source lists.
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The old Salts::Core package cannot coexist with the new SaltsCore
# package owner. Require one installed source-verified SaltsCore SDK.
find_package(SaltsCore 0.1 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(plutosvg CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
# This existing Linux Mesa/Xvfb qualification uses server-injected XTest
# events. Failure to find XTest is fatal; no direct-callback substitute.
find_package(X11 REQUIRED)
find_path(TANGO_XTEST_INCLUDE_DIR NAMES X11/extensions/XTest.h REQUIRED)
find_library(TANGO_XTEST_LIBRARY NAMES Xtst REQUIRED)
find_path(STB_INCLUDE_DIRS NAMES stb_image.h PATH_SUFFIXES stb REQUIRED)
find_path(Stb_INCLUDE_DIR NAMES stb_sprintf.h PATH_SUFFIXES stb REQUIRED)
if(NOT TARGET Salts::Unicode OR NOT TARGET Salts::Platform OR
   NOT TARGET Salts::FS)
  message(FATAL_ERROR
    "Tango pixel smoke requires one installed SaltsCore Unicode/Platform/FS SDK")
endif()
if(NOT TARGET glad::glad OR NOT TARGET glfw)
  message(FATAL_ERROR "Tango pixel smoke requires canonical glad and GLFW targets")
endif()

set(BUILD_TESTING OFF)
set(GCANVAS_BUILD_OPENGL ON CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_GLES OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_ANDROID_EGL OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_SVG ON CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_WINDOW OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_GPU_TESTS OFF CACHE BOOL "" FORCE)
set(GCANVAS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLEX_BUILD_BACKEND_TANGO_OPENGL ON)
set(FLEX_OPENGL_RENDER_ENGINE "gcanvas")
set(FLEX_ENGINE_INCLUDE_DIR "${STUN_ROOT}/flex/include")

add_subdirectory("${STUN_ROOT}/shared/gcanvas" gcanvas)
add_subdirectory("${STUN_ROOT}/tango" tango)
add_subdirectory("${STUN_ROOT}/flex/modules/render" flex_render)
add_subdirectory("${STUN_ROOT}/flex/render/engines/gcanvas" flex_gcanvas)
add_subdirectory("${STUN_ROOT}/flex/backends/opengl" flex_opengl)
add_subdirectory("${STUN_ROOT}/flex/backends/tango_opengl" tango_gl)

if(NOT TARGET flex_backend_tango_opengl)
  message(FATAL_ERROR "TangoOpenGL real source target was not created")
endif()

add_executable(tango_gl_real_pixel_smoke tango_gl_real_pixel_smoke.cpp)
target_compile_features(tango_gl_real_pixel_smoke PRIVATE cxx_std_17)
target_include_directories(tango_gl_real_pixel_smoke PRIVATE
  "${TANGO_XTEST_INCLUDE_DIR}")
target_compile_options(tango_gl_real_pixel_smoke PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>)
target_link_libraries(tango_gl_real_pixel_smoke PRIVATE
  flex_backend_tango_opengl glfw OpenGL::GL X11::X11
  "${TANGO_XTEST_LIBRARY}")
enable_testing()

set(TANGO_PIXEL_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    CACHE FILEPATH "Exact regular mono font used in actual TangoGL pixel acceptance")
set(TANGO_PIXEL_BOLD_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
    CACHE FILEPATH "Exact bold mono font for the same GPU presenter")
if(NOT EXISTS "${TANGO_PIXEL_FONT}" OR NOT EXISTS "${TANGO_PIXEL_BOLD_FONT}")
  message(FATAL_ERROR "TangoGL native pixel test requires both exact regular and bold font files")
endif()

add_test(NAME tango_gl_real_pixel
  COMMAND tango_gl_real_pixel_smoke
    "${TANGO_PIXEL_FONT}" "${TANGO_PIXEL_BOLD_FONT}"
    "${CMAKE_BINARY_DIR}/tango-gl-frame.ppm")
set_tests_properties(tango_gl_real_pixel PROPERTIES
  TIMEOUT 90 LABELS "gpu;opengl;tango")
