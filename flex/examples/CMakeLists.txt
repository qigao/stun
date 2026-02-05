# Flex Examples - Simplified with file discovery

# Find all .flex files
file(GLOB_RECURSE FLEX_FILES *.flex)
foreach(FLEX_FILE ${FLEX_FILES})
  get_filename_component(FLEX_NAME ${FLEX_FILE} NAME_WE)
  configure_file(${FLEX_FILE}
                 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${FLEX_NAME}.flex COPYONLY)
endforeach()

# Dependencies
find_package(SDL2 CONFIG QUIET)
find_package(glfw3 CONFIG QUIET)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG QUIET)

# Find all cpp files and create executables
file(GLOB CPP_FILES *.cpp)

foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(CPP_NAME ${CPP_FILE} NAME_WE)
  add_executable(${CPP_NAME} ${CPP_FILE})
  target_link_libraries(${CPP_NAME} PRIVATE flex OpenGL::GL)
  
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

  