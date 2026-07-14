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

# Find all cpp files and create executables
file(GLOB CPP_FILES *.cpp)

foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(CPP_NAME ${CPP_FILE} NAME_WE)
  add_executable(${CPP_NAME} ${CPP_FILE})
  target_link_libraries(${CPP_NAME} PRIVATE flex ${FLEX_EXAMPLES_OPENGL_LIB})

  if(CPP_NAME STREQUAL "tui_flex_demo")
    if(TARGET flex_backend_tui)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_tui)
    endif()
  elseif(CPP_NAME STREQUAL "nanovg_glfw_demo")
    if(TARGET flex_backend_nanovg)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_nanovg)
    endif()
  elseif(CPP_NAME STREQUAL "direct2d_hwnd_demo")
    if(TARGET flex_backend_d2d)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_d2d)
    endif()
  else()
    if(TARGET flex_backend_thorvg)
      target_link_libraries(${CPP_NAME} PRIVATE flex_backend_thorvg)
    endif()
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

  
