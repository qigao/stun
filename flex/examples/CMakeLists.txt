# Flex Examples - Simplified with file discovery

# Find all .flex files
file(GLOB_RECURSE FLEX_FILES *.flex)
foreach(FLEX_FILE ${FLEX_FILES})
  get_filename_component(FLEX_NAME ${FLEX_FILE} NAME_WE)
 
endforeach()

file(GLOB_RECURSE FLEX_SVG_FILES *.svg)
foreach(FLEX_SVG_FILE ${FLEX_SVG_FILES})
  get_filename_component(FLEX_SVG_NAME ${FLEX_SVG_FILE} NAME) 
endforeach()

# Dependencies
find_package(SDL2 CONFIG QUIET)
find_package(glfw3 CONFIG QUIET)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG QUIET)

set(FLEX_EXAMPLES_OPENGL_LIB)
if(TARGET OpenGL::GL)
  list(APPEND FLEX_EXAMPLES_OPENGL_LIB OpenGL::GL)
elseif(TARGET OpenGL::OpenGL)
  list(APPEND FLEX_EXAMPLES_OPENGL_LIB OpenGL::OpenGL)
elseif(WIN32)
  list(APPEND FLEX_EXAMPLES_OPENGL_LIB opengl32)
endif()

# Build only examples backed by the public OpenGL, Vulkan, TUI or Direct2D
# contracts. Retired ThorVG hosts are archived under legacy/thorvg and are
# intentionally not part of the supported build.
set(CPP_FILES
  component_binding_demo.cpp
  direct2d_hwnd_demo.cpp
  opengl_glfw_demo.cpp
  runtime_system_demo.cpp
  tui_flex_demo.cpp)

if(TARGET flex_render_engine_gcanvas AND TARGET gcanvas_window)
  list(APPEND CPP_FILES loading_animation_demo.cpp)
endif()

foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(CPP_NAME ${CPP_FILE} NAME_WE)
  add_executable(${CPP_NAME} ${CPP_FILE})
  target_link_libraries(${CPP_NAME} PRIVATE flex ${FLEX_EXAMPLES_OPENGL_LIB})

  if(CPP_NAME STREQUAL "tui_flex_demo")
    if(TARGET flex_backend_tui)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_tui)
    endif()
  elseif(CPP_NAME STREQUAL "opengl_glfw_demo")
    if(TARGET flex_backend_opengl)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_opengl)
    endif()
  elseif(CPP_NAME STREQUAL "direct2d_hwnd_demo")
    if(TARGET flex_backend_d2d)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_d2d)
    endif()
  elseif(CPP_NAME STREQUAL "loading_animation_demo")
    target_link_libraries(${CPP_NAME} PRIVATE flex_render_engine_gcanvas gCanvas::Window)
    target_compile_definitions(${CPP_NAME} PRIVATE
      FLEX_EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  
  # Link tango for tango backend examples
    target_link_libraries(${CPP_NAME} PRIVATE tango)
  
  # Link SDL2 if available
  if(TARGET SDL2::SDL2)
    target_link_libraries(${CPP_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
  endif()
  
  # Link GLFW if available
  if(TARGET glfw)
    target_link_libraries(${CPP_NAME} PRIVATE glfw)
  endif()
  
  # Link glad if available
  if(TARGET glad::glad)
    target_link_libraries(${CPP_NAME} PRIVATE glad::glad)
  endif()
  
  set_target_properties(${CPP_NAME} PROPERTIES FOLDER "flex/examples")
endforeach()

if(BUILD_TESTING AND TARGET loading_animation_demo)
  if(GCANVAS_BUILD_OPENGL)
    add_test(NAME flex_loading_animation_gcanvas_opengl
             COMMAND loading_animation_demo opengl --smoke)
    set_tests_properties(flex_loading_animation_gcanvas_opengl PROPERTIES
      LABELS "app;flex;gpu;opengl")
  endif()
  if(GCANVAS_BUILD_VULKAN)
    add_test(NAME flex_loading_animation_gcanvas_vulkan
             COMMAND loading_animation_demo vulkan --smoke)
    set_tests_properties(flex_loading_animation_gcanvas_vulkan PROPERTIES
      LABELS "app;flex;gpu;vulkan")
  endif()
endif()

  
