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
