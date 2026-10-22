add_library(flexUI_services STATIC service_registry.cpp)
add_library(FlexUI::Services ALIAS flexUI_services)

target_compile_features(flexUI_services PUBLIC cxx_std_17)
target_include_directories(
  flexUI_services
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
set_target_properties(
  flexUI_services PROPERTIES FOLDER "flexUI/services" EXPORT_NAME Services)

install(
  TARGETS flexUI_services
  EXPORT FlexUIServicesTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUIPlugin
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUIPlugin
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT FlexUIPlugin)
