# Requested examples link direct providers, no target sniff or GL fallback.
if(FLEX_BUILD_BACKEND_OPENGL)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(glad CONFIG REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)

  add_executable(visual_demo visual_demo.cpp)
  target_link_libraries(visual_demo PRIVATE flexUI flex_backend_opengl glfw glad::glad nlohmann_json::nlohmann_json OpenGL::GL)
  target_include_directories(visual_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
  set_target_properties(visual_demo PROPERTIES FOLDER flexUI/)

  add_executable(shadcn_ir_demo shadcn_ir_demo.cpp)
  target_link_libraries(shadcn_ir_demo PRIVATE flexUI flex_backend_opengl glfw glad::glad OpenGL::GL)
  target_include_directories(shadcn_ir_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
  set_target_properties(shadcn_ir_demo PROPERTIES FOLDER flexUI/)

  add_executable(markdown_demo markdown_demo.cpp)
  target_link_libraries(markdown_demo PRIVATE flexUI flex_backend_opengl glfw glad::glad OpenGL::GL)
  target_include_directories(markdown_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
  set_target_properties(markdown_demo PROPERTIES FOLDER flexUI/)
endif()

if(FLEX_BUILD_BACKEND_OPENGL)
  add_executable(opengl_demo opengl_demo.cpp)
  target_link_libraries(opengl_demo PRIVATE flexUI flex_backend_opengl glfw glad::glad OpenGL::GL)
  target_include_directories(opengl_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
  set_target_properties(opengl_demo PROPERTIES FOLDER flexUI/)
endif()

if(FLEX_BUILD_BACKEND_D2D)
  add_executable(direct2d_demo direct2d_demo.cpp)
  target_link_libraries(direct2d_demo PRIVATE flexUI flex_backend_d2d)
  target_include_directories(direct2d_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
  set_target_properties(direct2d_demo PROPERTIES FOLDER flexUI/)
endif()

if(FLEX_BUILD_BACKEND_TUI)
  add_executable(flexui_tui_demo tui_demo.cpp)
  target_link_libraries(flexui_tui_demo PRIVATE flexUI flex flex_backend_tui tango)
  target_include_directories(flexui_tui_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
  set_target_properties(flexui_tui_demo PROPERTIES FOLDER flexUI/)

  # The Streaming consumer uses Tango's native UI-owner loop, not the
  # historical dashboard's periodic polling loop.
  find_package(Threads REQUIRED)
  add_executable(flexui_tui_streaming_demo tui_streaming_demo.cpp)
  target_link_libraries(flexui_tui_streaming_demo PRIVATE
    flexUI flex_backend_tui tango Threads::Threads)
  set_target_properties(flexui_tui_streaming_demo PROPERTIES FOLDER flexUI/)
endif()

if(FLEXUI_ENABLE_TURBOSCRIPT)
  add_executable(desktop_xml_tbs_demo desktop_xml_tbs_demo.cpp)
  target_link_libraries(desktop_xml_tbs_demo
                        PRIVATE FlexUI::ControllerTurboScript)
  set_target_properties(desktop_xml_tbs_demo PROPERTIES FOLDER flexUI/)
endif()

if(FLEXUI_ENABLE_TURBOSCRIPT AND FLEXUI_ENABLE_PLUGINS AND
   GCANVAS_BUILD_WINDOW AND FLEX_BUILD_RENDER_ENGINE_GCANVAS)
  add_subdirectory(desktop_editor)
endif()
 
