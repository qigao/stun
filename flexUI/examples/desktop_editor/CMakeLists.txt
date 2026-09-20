add_library(flexui_document_service MODULE document_service.c)
target_link_libraries(flexui_document_service PRIVATE FlexUI::PluginSDK)
target_compile_definitions(flexui_document_service PRIVATE FLEXUI_PLUGIN_BUILD)
set_target_properties(
  flexui_document_service
  PROPERTIES C_STANDARD 11
             C_STANDARD_REQUIRED ON
             C_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON
             FOLDER "flexUI/examples/desktop_editor")

add_executable(flexui_desktop_editor main.cpp)
target_compile_features(flexui_desktop_editor PRIVATE cxx_std_17)
target_link_libraries(
  flexui_desktop_editor
  PRIVATE FlexUI::GCanvasPluginWindowHost FlexUI::ControllerTurboScript
          Salts::Core)
target_compile_definitions(
  flexui_desktop_editor
  PRIVATE
    FLEXUI_DESKTOP_EDITOR_ASSET_DIRECTORY_NAME="desktop_editor_assets"
    FLEXUI_DESKTOP_EDITOR_PLUGIN_FILE_NAME="$<TARGET_FILE_NAME:flexui_document_service>")
add_dependencies(flexui_desktop_editor flexui_document_service)
set_target_properties(
  flexui_desktop_editor PROPERTIES FOLDER "flexUI/examples/desktop_editor")

set(_flexui_desktop_editor_assets editor.xml editor.css editor.tbs)
set(_flexui_desktop_editor_asset_paths)
foreach(_flexui_desktop_editor_asset IN LISTS _flexui_desktop_editor_assets)
  list(APPEND _flexui_desktop_editor_asset_paths
       "${CMAKE_CURRENT_SOURCE_DIR}/${_flexui_desktop_editor_asset}")
endforeach()
add_custom_target(
  flexui_desktop_editor_assets
  COMMAND
    ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:flexui_desktop_editor>/desktop_editor_assets"
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    ${_flexui_desktop_editor_asset_paths}
    "$<TARGET_FILE_DIR:flexui_desktop_editor>/desktop_editor_assets"
  DEPENDS ${_flexui_desktop_editor_asset_paths}
  COMMENT "Deploying FlexUI desktop editor assets")
add_dependencies(flexui_desktop_editor flexui_desktop_editor_assets)
set_target_properties(
  flexui_desktop_editor_assets
  PROPERTIES FOLDER "flexUI/examples/desktop_editor")

install(
  TARGETS flexui_desktop_editor flexui_document_service
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  FILES ${_flexui_desktop_editor_asset_paths}
  DESTINATION ${CMAKE_INSTALL_BINDIR}/desktop_editor_assets)
