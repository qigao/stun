# Flex Examples - Simplified with file discovery

# Find all .flex files
file(GLOB_RECURSE FLEX_FILES *.flex)
list(LENGTH FLEX_FILES FLEX_COUNT)
foreach(FLEX_FILE ${FLEX_FILES})
  get_filename_component(FLEX_NAME ${FLEX_FILE} NAME_WE)
  configure_file(${FLEX_FILE}
                 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${FLEX_NAME}.flex COPYONLY)
endforeach()

# SDL2 Examples (require SDL2)
find_package(SDL2 CONFIG QUIET)
 
# Find remaining cpp files (those without _demo or _app suffix)
file(GLOB CPP_FILES *.cpp) 
list(LENGTH CPP_FILES CPP_COUNT)
math(EXPR TOTAL_COUNT "${DEMO_COUNT} + ${CPP_COUNT}")

foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(CPP_NAME ${CPP_FILE} NAME_WE)
  add_executable(${CPP_NAME} ${CPP_FILE})
  target_link_libraries(${CPP_NAME} PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(${CPP_NAME} PROPERTIES FOLDER "flex/examples")
endforeach()

  