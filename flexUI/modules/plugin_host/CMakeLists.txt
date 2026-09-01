add_library(flexUI_plugin_sdk INTERFACE)
add_library(FlexUI::PluginSDK ALIAS flexUI_plugin_sdk)

target_include_directories(
  flexUI_plugin_sdk
  INTERFACE $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>)
set_target_properties(flexUI_plugin_sdk PROPERTIES FOLDER "flexUI/plugin_host")
set_target_properties(flexUI_plugin_sdk PROPERTIES EXPORT_NAME PluginSDK)

install(
  TARGETS flexUI_plugin_sdk
  EXPORT FlexUIPluginTargets
  COMPONENT FlexUIPlugin)

if(FLEXUI_ENABLE_PLUGINS)
  add_library(
    flexUI_plugin_host STATIC
    native_plugin_library.cpp
    native_plugin_library.hpp
    plugin_dependency_resolver.cpp
    plugin_dependency_resolver.hpp
    plugin_host.cpp
    plugin_manifest.cpp
    plugin_manifest.hpp
    plugin_module.cpp
    plugin_module.hpp
    plugin_validation.hpp)
  add_library(FlexUI::PluginHost ALIAS flexUI_plugin_host)
  target_compile_features(flexUI_plugin_host PUBLIC cxx_std_17)
  target_include_directories(
    flexUI_plugin_host
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
           $<INSTALL_INTERFACE:include>)
  target_link_libraries(
    flexUI_plugin_host
    PUBLIC FlexUI::PluginSDK FlexUI::Services
    PRIVATE TurboUtils::Core TurboParser::Parser ${CMAKE_DL_LIBS})
  if(WIN32)
    target_compile_definitions(
      flexUI_plugin_host PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
  set_target_properties(
    flexUI_plugin_host PROPERTIES FOLDER "flexUI/plugin_host"
                                  EXPORT_NAME PluginHost)
  install(
    TARGETS flexUI_plugin_host
    EXPORT FlexUIPluginTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUIPlugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUIPlugin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT FlexUIPlugin)
endif()
