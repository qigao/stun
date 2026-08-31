add_library(flexUI_controller STATIC application_command.cpp controller.cpp
                                     mutation.cpp box_mutation_host.cpp)
add_library(FlexUI::Controller ALIAS flexUI_controller)

target_link_libraries(flexUI_controller PUBLIC FlexUI::Core)
target_include_directories(
  flexUI_controller
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
set_target_properties(flexUI_controller PROPERTIES FOLDER "flexUI/controller")

install(TARGETS flexUI_controller DESTINATION lib)
