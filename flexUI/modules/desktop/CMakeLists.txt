add_library(flexUI_gcanvas_input STATIC gcanvas_input.cpp)
add_library(FlexUI::GCanvasInput ALIAS flexUI_gcanvas_input)

target_link_libraries(flexUI_gcanvas_input PUBLIC FlexUI::Core gCanvas::Core)
target_include_directories(
  flexUI_gcanvas_input
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
set_target_properties(flexUI_gcanvas_input PROPERTIES FOLDER "flexUI/desktop")

install(TARGETS flexUI_gcanvas_input DESTINATION lib)

add_library(flexUI_gcanvas_application_input STATIC gcanvas_application_input.cpp)
add_library(FlexUI::GCanvasApplicationInput ALIAS flexUI_gcanvas_application_input)

target_link_libraries(
  flexUI_gcanvas_application_input
  PUBLIC FlexUI::Controller FlexUI::GCanvasInput)
target_include_directories(
  flexUI_gcanvas_application_input
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
set_target_properties(
  flexUI_gcanvas_application_input PROPERTIES FOLDER "flexUI/desktop")

install(TARGETS flexUI_gcanvas_application_input DESTINATION lib)

if(TARGET gCanvas::Window AND TARGET flex_render_engine_gcanvas)
  add_library(flexUI_gcanvas_window_host STATIC gcanvas_window_host.cpp)
  add_library(FlexUI::GCanvasWindowHost ALIAS flexUI_gcanvas_window_host)

  target_link_libraries(
    flexUI_gcanvas_window_host
    PUBLIC FlexUI::GCanvasApplicationInput gCanvas::Window
    PRIVATE flex_render_engine_gcanvas)
  target_include_directories(
    flexUI_gcanvas_window_host
    PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
           $<INSTALL_INTERFACE:include>)
  target_compile_features(flexUI_gcanvas_window_host PUBLIC cxx_std_17)
  set_target_properties(
    flexUI_gcanvas_window_host PROPERTIES FOLDER "flexUI/desktop")

  install(TARGETS flexUI_gcanvas_window_host DESTINATION lib)

  if(TARGET FlexUI::PluginHost)
    add_library(flexUI_gcanvas_plugin_window_host STATIC
                gcanvas_plugin_window_host.cpp)
    add_library(FlexUI::GCanvasPluginWindowHost ALIAS
                flexUI_gcanvas_plugin_window_host)
    target_link_libraries(
      flexUI_gcanvas_plugin_window_host
      PUBLIC FlexUI::GCanvasWindowHost FlexUI::PluginHost)
    target_include_directories(
      flexUI_gcanvas_plugin_window_host
      PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
             $<INSTALL_INTERFACE:include>)
    target_compile_features(flexUI_gcanvas_plugin_window_host PUBLIC cxx_std_17)
    set_target_properties(
      flexUI_gcanvas_plugin_window_host
      PROPERTIES FOLDER "flexUI/desktop")

    install(TARGETS flexUI_gcanvas_plugin_window_host DESTINATION lib)
  endif()
endif()
