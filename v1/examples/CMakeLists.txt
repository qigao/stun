file(GLOB SOURCES example*.cpp)
find_package(lunasvg)
foreach(example_source ${SOURCES})
  get_filename_component(source_name ${example_source} NAME_WE)
  set(EXAMPLE_NAME ${source_name})

  add_executable(${EXAMPLE_NAME} ${example_source})
  target_link_libraries(
    ${EXAMPLE_NAME}
    PRIVATE fmtlog::fmtlog
            nanogui
            nanovg::nanovg
            lunasvg::lunasvg
            NanoGUI::M3Theme
            NanoGUI::AppleTheme
            NanoGUI::FluentIOSTheme
            NanoGUI::FluentWebTheme)

  target_include_directories(${EXAMPLE_NAME} PRIVATE ../include)

  set_target_properties(${EXAMPLE_NAME} PROPERTIES FOLDER nanogui/example)

endforeach()
add_subdirectory(sdl)
