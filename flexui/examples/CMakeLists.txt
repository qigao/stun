file(GLOB SOURCES *.cpp)

foreach(example_source ${SOURCES})
  get_filename_component(source_name ${example_source} NAME_WE)
  set(EXAMPLE_NAME ${source_name})

  add_executable(${EXAMPLE_NAME} ${example_source})
  target_link_libraries(  ${EXAMPLE_NAME}
    PRIVATE fmtlog::fmtlog
            nanovg_css
            flexui_core
           )
  target_include_directories(${EXAMPLE_NAME} PRIVATE ../include)

  set_target_properties(${EXAMPLE_NAME} PROPERTIES FOLDER flexui/example)

endforeach()