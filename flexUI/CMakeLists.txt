# flexUI - UI Framework

cmake_minimum_required(VERSION 3.10)

project(flexUI)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(include)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

set(FLEXUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(FLEXUI_INCLUDE_DIR "${FLEXUI_ROOT}/include")
add_subdirectory(modules/tailwindcss)

# Canonical REQUIRED dependency resolution; compiler/linker own the failure.
find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/core/base.h REQUIRED)
find_library(LEXBOR_LIBRARY NAMES lexbor lexbor_static NAMES_PER_DIR REQUIRED)
find_package(unofficial-tree-sitter CONFIG REQUIRED)
include_directories(${LEXBOR_INCLUDE_DIR})

set(FLEXUI_UTILITY_WHITELIST
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/shadcn-ir/schema/utility_whitelist.json")
set(FLEXUI_DEFAULT_THEME
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/default_theme.css")
set_property(
  DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${FLEXUI_UTILITY_WHITELIST}"
  "${FLEXUI_DEFAULT_THEME}")
file(READ "${FLEXUI_UTILITY_WHITELIST}"
     FLEXUI_DEFAULT_UTILITY_WHITELIST_JSON)
file(READ "${FLEXUI_DEFAULT_THEME}" FLEXUI_DEFAULT_THEME_CSS)

# Keep each generated literal below MSVC's per-literal size limit. Adjacent C++
# string literals are concatenated by the compiler without changing the asset.
function(flexui_make_cpp_raw_string input output)
  set(chunk_size 8192)
  string(LENGTH "${input}" input_length)
  set(offset 0)
  set(result "")
  while(offset LESS input_length)
    math(EXPR remaining "${input_length} - ${offset}")
    if(remaining GREATER chunk_size)
      set(length ${chunk_size})
    else()
      set(length ${remaining})
    endif()
    string(SUBSTRING "${input}" ${offset} ${length} chunk)
    string(APPEND result "R\"FLEXUI_ASSET(${chunk})FLEXUI_ASSET\"\n")
    math(EXPR offset "${offset} + ${length}")
  endwhile()
  set(${output} "${result}" PARENT_SCOPE)
endfunction()

flexui_make_cpp_raw_string("${FLEXUI_DEFAULT_UTILITY_WHITELIST_JSON}"
                           FLEXUI_DEFAULT_UTILITY_WHITELIST_CPP)
flexui_make_cpp_raw_string("${FLEXUI_DEFAULT_THEME_CSS}"
                           FLEXUI_DEFAULT_THEME_CPP)
set(FLEXUI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FLEXUI_GENERATED_DIR}")
configure_file(
  src/default_style_assets.cpp.in
  "${FLEXUI_GENERATED_DIR}/default_style_assets.cpp"
  @ONLY)


# 源文件
set(FLEXUI_SOURCES
  "${FLEXUI_GENERATED_DIR}/default_style_assets.cpp"
  src/box.cpp
  src/binding_runtime.cpp
  src/ui_document.cpp
  src/ui_xml.cpp
  src/ui_xml_admission.cpp
  src/widget_registry.cpp
  src/keyed_repeater.cpp
  src/view_pipeline.cpp
  src/element.cpp
  src/widget.cpp
  src/computed_style.cpp
  src/style_engine.cpp
  src/style_property_registry.cpp
  src/style_property_impact.cpp
  src/css_semantic_value.cpp
  src/css_typed_value.cpp
  src/clipboard.cpp
  src/textedit.cpp
  src/transition.cpp
  src/text_util.cpp
  src/text_layout.cpp
  src/event_dispatcher.cpp
  src/layout_manager.cpp
  src/render_manager.cpp
  src/render_command.cpp
  src/shadcn_ir.cpp
  src/syntax_highlighter.cpp
  src/highlighters/treesitter_highlighter.cpp
  src/highlighters/ts_plugin_loader.cpp
  src/widgets/input_widget.cpp
  src/widgets/button_widget.cpp
  src/widgets/label_widget.cpp
  src/widgets/checkbox_widget.cpp
  src/widgets/slider_widget.cpp
  src/widgets/progressbar_widget.cpp
  src/widgets/radio_widget.cpp
  src/widgets/textarea_widget.cpp
  src/widgets/select_widget.cpp
  src/widgets/switch_widget.cpp
  src/widgets/tooltip_widget.cpp
  src/widgets/modal_widget.cpp
  src/widgets/tabs_widget.cpp
  src/widgets/image_widget.cpp
  src/widgets/badge_widget.cpp
  src/widgets/spinner_widget.cpp
  src/widgets/divider_widget.cpp
  src/widgets/toast_widget.cpp
  src/widgets/dropdown_widget.cpp
  src/widgets/accordion_widget.cpp
  src/widgets/calendar_widget.cpp
  src/widgets/table_widget.cpp
  src/widgets/tree_widget.cpp
  src/widgets/colorpicker_widget.cpp
  src/widgets/group_button_widget.cpp
  src/widgets/toggle_group_widget.cpp
  src/widgets/avatar_widget.cpp
  src/widgets/card_widget.cpp
  src/widgets/breadcrumb_widget.cpp
  src/widgets/pagination_widget.cpp
  src/widgets/stepper_widget.cpp
  src/widgets/gradient_editor_widget.cpp
  src/widgets/markdown_widget.cpp
)

# 头文件
set(FLEXUI_HEADERS
  include/flexUI/types.h
  include/flexUI/ui_handle.h
  include/flexUI/binding_runtime.h
  include/flexUI/ui_document.h
  include/flexUI/ui_xml.h
  include/flexUI/ui_node_schema.h
  include/flexUI/widget_registry.h
  include/flexUI/keyed_repeater.h
  include/flexUI/text_value_widget.h
  include/flexUI/computed_style.h
  include/flexUI/style_property.h
  include/flexUI/style_engine.h
  include/flexUI/event.h
  include/flexUI/event_dispatcher.h
  include/flexUI/view_pipeline.h
  include/flexUI/layout_manager.h
  include/flexUI/render_manager.h
  include/flexUI/render_command.h
  include/flexUI/render_frame.h
  include/flexUI/shadcn_ir.h
  include/flexUI/tailwindcss.h
  include/flexUI/utility_jit.h
  include/flexUI/widget.h
  include/flexUI/element.h
  include/flexUI/box.h
  include/flexUI/textedit.h
  include/flexUI/transition.h
  include/flexUI/text_layout.h
  include/flexUI/text_util.h
  include/flexUI/renderer.h
  include/flexUI/group.h
  include/flexUI/shapes.h
  include/flexUI/widgets/input_widget.h
  include/flexUI/widgets/label_widget.h
  include/flexUI/widgets/checkbox_widget.h
  include/flexUI/widgets/slider_widget.h
  include/flexUI/widgets/progressbar_widget.h
  include/flexUI/widgets/button_widget.h
  include/flexUI/widgets/radio_widget.h
  include/flexUI/widgets/textarea_widget.h
  include/flexUI/widgets/select_widget.h
  include/flexUI/widgets/tooltip_widget.h
  include/flexUI/widgets/modal_widget.h
  include/flexUI/widgets/tabs_widget.h
  include/flexUI/widgets/switch_widget.h
  include/flexUI/widgets/image_widget.h
  include/flexUI/widgets/badge_widget.h
  include/flexUI/widgets/spinner_widget.h
  include/flexUI/widgets/divider_widget.h
  include/flexUI/widgets/toast_widget.h
  include/flexUI/widgets/dropdown_widget.h
  include/flexUI/widgets/accordion_widget.h
  include/flexUI/widgets/calendar_widget.h
  include/flexUI/widgets/table_widget.h
  include/flexUI/widgets/tree_widget.h
  include/flexUI/widgets/colorpicker_widget.h
  include/flexUI/widgets/group_button_widget.h
  include/flexUI/widgets/toggle_group_widget.h
  include/flexUI/widgets/avatar_widget.h
  include/flexUI/widgets/card_widget.h
  include/flexUI/widgets/breadcrumb_widget.h
  include/flexUI/widgets/pagination_widget.h
  include/flexUI/widgets/stepper_widget.h
  include/flexUI/widgets/gradient_editor_widget.h
  include/flexUI/widgets/markdown_widget.h
)

# 创建库
add_library(flexUI STATIC ${FLEXUI_SOURCES} ${FLEXUI_HEADERS})
add_library(FlexUI::Core ALIAS flexUI)
set_target_properties(flexUI PROPERTIES EXPORT_NAME Core)

get_filename_component(_flexui_lexbor_library_name "${LEXBOR_LIBRARY}" NAME_WE)
set(_flexui_lexbor_install_target lexbor::lexbor)
if(_flexui_lexbor_library_name MATCHES "^(lib)?lexbor_static$")
  set(_flexui_lexbor_install_target lexbor::lexbor_static)
  target_compile_definitions(flexUI PRIVATE LEXBOR_STATIC)
endif()

# Windows: disable min/max macros that conflict with std::min/std::max
if(WIN32)
    target_compile_definitions(flexUI PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# 链接依赖
target_link_libraries(flexUI 
    PUBLIC $<BUILD_INTERFACE:${LEXBOR_LIBRARY}>
           $<INSTALL_INTERFACE:${_flexui_lexbor_install_target}>
           Flex::Runtime Flex::Render Flex::Animation FlexUI::TailwindCSS Salts::Core Salts::CMeta nlohmann_json::nlohmann_json
    PRIVATE Flex::Compiler Salts::Unicode unofficial::tree-sitter::tree-sitter pugixml::pugixml
)
# Opt-in author-source Jinja XML compiler. Ordinary FlexUI builds do not
# link the Salts Jinja provider or a second XML parser.
option(FLEXUI_ENABLE_JINJA_XML
       "Build the separately linked Jinja XML source admission library" OFF)
if(FLEXUI_ENABLE_JINJA_XML)
  add_library(flexUI_jinja_xml STATIC
      src/ui_jinja_xml.cpp include/flexUI/ui_jinja_xml.h)
  add_library(FlexUI::JinjaXml ALIAS flexUI_jinja_xml)
  target_compile_features(flexUI_jinja_xml PUBLIC cxx_std_17)
  # Missing explicitly required targets fail at configure/generation/link.
  # No if(TARGET) availability probe, source copy or fallback provider.
  target_link_libraries(flexUI_jinja_xml
      PUBLIC FlexUI::Core
      PRIVATE Salts::JinjaCMeta)
  target_include_directories(flexUI_jinja_xml
      PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
             $<INSTALL_INTERFACE:include>)
  set_target_properties(flexUI_jinja_xml PROPERTIES
      FOLDER "flexUI/jinja" EXPORT_NAME JinjaXml)
  install(TARGETS flexUI_jinja_xml EXPORT FlexUIJinjaXmlTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT FlexUICore)
endif()

set_target_properties(flexUI PROPERTIES FOLDER flexUI/)
target_include_directories(
  flexUI
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/modules/tailwindcss
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tools/tree-sitter/plugins>
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/vendor/tinyfsm/>
         $<BUILD_INTERFACE:${Stb_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)

if(WIN32)
  add_custom_command(
    TARGET flexUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Salts::Core>
            $<TARGET_FILE_DIR:flexUI>
    COMMENT "Copying the Salts Core runtime beside FlexUI executables")
endif()

add_subdirectory(modules/services)
add_subdirectory(modules/plugin_host)
add_subdirectory(modules/controller)
add_subdirectory(modules/desktop)

set(FLEXUI_PLUGIN_PACKAGE_VERSION "1.0.0")
set(FLEXUI_CONFIG_HAS_PLUGIN_HOST "${FLEXUI_ENABLE_PLUGINS}")
set(FLEXUI_CONFIG_HAS_JINJA_XML "${FLEXUI_ENABLE_JINJA_XML}")
configure_package_config_file(
  cmake/FlexUIConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/FlexUIConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/FlexUIConfigVersion.cmake"
  VERSION "${FLEXUI_PLUGIN_PACKAGE_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(
  EXPORT FlexUIPluginSDKTargets
  FILE FlexUIPluginSDKTargets.cmake
  NAMESPACE FlexUI::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
  COMPONENT FlexUIPlugin)
install(
  EXPORT FlexUIServicesTargets
  FILE FlexUIServicesTargets.cmake
  NAMESPACE FlexUI::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
  COMPONENT FlexUIPlugin)
if(FLEXUI_ENABLE_PLUGINS)
  install(
    EXPORT FlexUIPluginHostTargets
    FILE FlexUIPluginHostTargets.cmake
    NAMESPACE FlexUI::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
    COMPONENT FlexUIPlugin)
endif()
install(
  EXPORT FlexUICoreTargets
  FILE FlexUICoreTargets.cmake
  NAMESPACE FlexUI::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
  COMPONENT FlexUICore)
if(FLEXUI_ENABLE_JINJA_XML)
  install(
    EXPORT FlexUIJinjaXmlTargets
    FILE FlexUIJinjaXmlTargets.cmake
    NAMESPACE FlexUI::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
    COMPONENT FlexUICore)
endif()
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/FlexUIConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FlexUIConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
  COMPONENT FlexUIPlugin)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/FlexUIConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FlexUIConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
  COMPONENT FlexUICore)
install(
  FILES include/flexUI/application_command.h
        include/flexUI/application_completion.h
        include/flexUI/application_service.h
        include/flexUI/plugin_abi.h
        include/flexUI/plugin_host.h
        include/flexUI/service_registry.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/flexUI"
  COMPONENT FlexUIPlugin)


# 安装头文件
install(DIRECTORY include/flexUI DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT FlexUICore)
install(FILES include/flexUI.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT FlexUICore)

# 安装库
set_target_properties(flexUI_tailwindcss PROPERTIES EXPORT_NAME TailwindCSS)
install(TARGETS flexUI flexUI_tailwindcss EXPORT FlexUICoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT FlexUICore)

if(NANOGUI_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
