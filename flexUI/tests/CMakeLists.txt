enable_testing()

find_package(nlohmann_json CONFIG REQUIRED)

function(flexui_configure_tinytest target_name)
  target_link_libraries(${target_name} PRIVATE Salts::TinyTest)
endfunction()

add_executable(test_plugin_abi test_plugin_abi.c)
target_link_libraries(test_plugin_abi PRIVATE FlexUI::PluginSDK)
flexui_configure_tinytest(test_plugin_abi)
set_target_properties(
  test_plugin_abi PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
                             FOLDER "flexUI/tests")
add_test(NAME test_plugin_abi COMMAND test_plugin_abi)

if(FLEXUI_ENABLE_PLUGINS)
  include("${CMAKE_CURRENT_LIST_DIR}/plugin-native-targets.cmake")
  flexui_add_native_plugin_tests()
endif()

add_executable(test_markdown_widget test_markdown_widget.cpp)
target_link_libraries(test_markdown_widget PRIVATE flexUI)
flexui_configure_tinytest(test_markdown_widget)
target_include_directories(test_markdown_widget PRIVATE ../include)
set_target_properties(test_markdown_widget PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_markdown_widget COMMAND test_markdown_widget)

# A real DSO lease test runs only when the tree-sitter C language plugin is
# part of this build. Standalone FlexUI-only consumers stay independent.
if(TARGET ts-lang-c)
  add_executable(test_ts_highlighter_dso_lifetime
                 test_highlighter_dso_lifetime.cpp)
  target_link_libraries(test_ts_highlighter_dso_lifetime PRIVATE flexUI)
  flexui_configure_tinytest(test_ts_highlighter_dso_lifetime)
  target_include_directories(test_ts_highlighter_dso_lifetime PRIVATE ../include)
  target_compile_definitions(test_ts_highlighter_dso_lifetime PRIVATE
    "TS_LANG_C_PATH=\"$<TARGET_FILE:ts-lang-c>\"")
  add_dependencies(test_ts_highlighter_dso_lifetime ts-lang-c)
  set_target_properties(test_ts_highlighter_dso_lifetime
                        PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_ts_highlighter_dso_lifetime
           COMMAND test_ts_highlighter_dso_lifetime)
endif()

add_executable(test_style_engine test_style_engine.cpp)
target_link_libraries(test_style_engine PRIVATE flexUI)
flexui_configure_tinytest(test_style_engine)
target_include_directories(test_style_engine PRIVATE ../include)
set_target_properties(test_style_engine PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_style_engine COMMAND test_style_engine)

add_executable(test_style_property_registry test_style_property_registry.cpp
               ${CMAKE_CURRENT_LIST_DIR}/../../flex/tests/cmeta_types_peer.cpp)
target_link_libraries(test_style_property_registry PRIVATE flexUI)
flexui_configure_tinytest(test_style_property_registry)
target_include_directories(test_style_property_registry PRIVATE ../include)
set_target_properties(test_style_property_registry PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_style_property_registry COMMAND test_style_property_registry)

add_executable(test_css_typed_ir test_css_typed_ir.cpp)
target_link_libraries(test_css_typed_ir PRIVATE flexUI)
flexui_configure_tinytest(test_css_typed_ir)
target_include_directories(test_css_typed_ir PRIVATE ../include)
set_target_properties(test_css_typed_ir PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_css_typed_ir COMMAND test_css_typed_ir)

add_executable(test_css_semantic_value test_css_semantic_value.cpp)
target_link_libraries(test_css_semantic_value PRIVATE flexUI)
flexui_configure_tinytest(test_css_semantic_value)
target_include_directories(test_css_semantic_value PRIVATE ../include)
set_target_properties(test_css_semantic_value PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_css_semantic_value COMMAND test_css_semantic_value)

add_executable(test_css_typed_apply test_css_typed_apply.cpp)
target_link_libraries(test_css_typed_apply PRIVATE flexUI)
flexui_configure_tinytest(test_css_typed_apply)
target_include_directories(test_css_typed_apply PRIVATE ../include)
set_target_properties(test_css_typed_apply PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_css_typed_apply COMMAND test_css_typed_apply)

add_executable(test_typed_transition test_typed_transition.cpp
               ${CMAKE_CURRENT_LIST_DIR}/../../flex/tests/cmeta_types_peer.cpp)
target_link_libraries(test_typed_transition PRIVATE flexUI)
flexui_configure_tinytest(test_typed_transition)
target_include_directories(test_typed_transition PRIVATE ../include)
set_target_properties(test_typed_transition PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_typed_transition COMMAND test_typed_transition)

add_executable(test_typed_animation test_typed_animation.cpp
               ${CMAKE_CURRENT_LIST_DIR}/../../flex/tests/cmeta_types_peer.cpp)
target_link_libraries(test_typed_animation PRIVATE flexUI)
flexui_configure_tinytest(test_typed_animation)
target_include_directories(test_typed_animation PRIVATE ../include)
set_target_properties(test_typed_animation PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_typed_animation COMMAND test_typed_animation)

add_executable(test_text_layout_unicode test_text_layout_unicode.cpp)
target_link_libraries(test_text_layout_unicode PRIVATE flexUI)
flexui_configure_tinytest(test_text_layout_unicode)
set_target_properties(test_text_layout_unicode PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_text_layout_unicode COMMAND test_text_layout_unicode)

add_executable(test_text_util_unicode test_text_util_unicode.cpp)
target_link_libraries(test_text_util_unicode PRIVATE flexUI)
flexui_configure_tinytest(test_text_util_unicode)
set_target_properties(test_text_util_unicode PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_text_util_unicode COMMAND test_text_util_unicode)


add_executable(test_unicode_boundaries test_unicode_boundaries.cpp)
target_link_libraries(test_unicode_boundaries PRIVATE flexUI)
flexui_configure_tinytest(test_unicode_boundaries)
set_target_properties(test_unicode_boundaries PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_unicode_boundaries COMMAND test_unicode_boundaries)

add_executable(test_render_semantics test_render_semantics.cpp)
target_link_libraries(test_render_semantics PRIVATE flexUI)
flexui_configure_tinytest(test_render_semantics)
target_include_directories(test_render_semantics PRIVATE ../include)
set_target_properties(test_render_semantics PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_render_semantics COMMAND test_render_semantics)

add_executable(test_render_command test_render_command.cpp)
target_link_libraries(test_render_command PRIVATE flexUI)
flexui_configure_tinytest(test_render_command)
target_include_directories(test_render_command PRIVATE ../include)
set_target_properties(test_render_command PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_render_command COMMAND test_render_command)

add_executable(test_widget_contracts test_widget_contracts.cpp)
target_link_libraries(test_widget_contracts PRIVATE flexUI)
flexui_configure_tinytest(test_widget_contracts)
target_include_directories(test_widget_contracts PRIVATE ../include)
set_target_properties(test_widget_contracts PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_widget_contracts COMMAND test_widget_contracts)

add_executable(test_host_bridge test_host_bridge.cpp)
target_link_libraries(test_host_bridge PRIVATE flexUI)
flexui_configure_tinytest(test_host_bridge)
target_include_directories(test_host_bridge PRIVATE ../include)
set_target_properties(test_host_bridge PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_host_bridge COMMAND test_host_bridge)

add_executable(test_event_dispatcher test_event_dispatcher.cpp)
target_link_libraries(test_event_dispatcher PRIVATE flexUI)
flexui_configure_tinytest(test_event_dispatcher)
target_include_directories(test_event_dispatcher PRIVATE ../include)
set_target_properties(test_event_dispatcher PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_event_dispatcher COMMAND test_event_dispatcher)

add_executable(test_shadcn_conformance test_shadcn_conformance.cpp)
target_link_libraries(test_shadcn_conformance
                      PRIVATE flexUI nlohmann_json::nlohmann_json)
flexui_configure_tinytest(test_shadcn_conformance)
target_include_directories(test_shadcn_conformance PRIVATE ../include)
target_compile_definitions(
  test_shadcn_conformance
  PRIVATE FLEXUI_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(test_shadcn_conformance PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_shadcn_conformance COMMAND test_shadcn_conformance)

add_executable(test_shadcn_ir_fixtures test_shadcn_ir_fixtures.cpp)
target_link_libraries(test_shadcn_ir_fixtures
                      PRIVATE flexUI nlohmann_json::nlohmann_json)
flexui_configure_tinytest(test_shadcn_ir_fixtures)
target_include_directories(test_shadcn_ir_fixtures PRIVATE ../include)
target_compile_definitions(
  test_shadcn_ir_fixtures
  PRIVATE FLEXUI_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(test_shadcn_ir_fixtures PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_shadcn_ir_fixtures COMMAND test_shadcn_ir_fixtures)

add_executable(test_utility_jit test_utility_jit.cpp)
target_link_libraries(test_utility_jit
                      PRIVATE flexUI nlohmann_json::nlohmann_json)
flexui_configure_tinytest(test_utility_jit)
target_include_directories(test_utility_jit PRIVATE ../include)
set_target_properties(test_utility_jit PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_utility_jit COMMAND test_utility_jit)

add_executable(test_tailwindcss_module test_tailwindcss_module.cpp)
target_link_libraries(test_tailwindcss_module PRIVATE FlexUI::TailwindCSS)
flexui_configure_tinytest(test_tailwindcss_module)
set_target_properties(test_tailwindcss_module PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_tailwindcss_module COMMAND test_tailwindcss_module)

add_executable(utility_jit_benchmark utility_jit_benchmark.cpp)
target_link_libraries(utility_jit_benchmark PRIVATE flexUI)
target_include_directories(utility_jit_benchmark PRIVATE ../include)
set_target_properties(utility_jit_benchmark PROPERTIES FOLDER "flexUI/benchmarks")

add_executable(test_binding_runtime test_binding_runtime.cpp)
target_link_libraries(test_binding_runtime PRIVATE flexUI)
flexui_configure_tinytest(test_binding_runtime)
target_include_directories(test_binding_runtime PRIVATE ../include)
set_target_properties(test_binding_runtime PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_binding_runtime COMMAND test_binding_runtime)

add_executable(test_ui_document test_ui_document.cpp)
target_link_libraries(test_ui_document PRIVATE flexUI)
flexui_configure_tinytest(test_ui_document)
target_include_directories(test_ui_document PRIVATE ../include)
set_target_properties(test_ui_document PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_ui_document COMMAND test_ui_document)

add_executable(test_ui_xml test_ui_xml.cpp)
target_link_libraries(test_ui_xml PRIVATE flexUI)
flexui_configure_tinytest(test_ui_xml)
target_include_directories(test_ui_xml PRIVATE ../include)
set_target_properties(test_ui_xml PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_ui_xml COMMAND test_ui_xml)

add_executable(test_widget_registry test_widget_registry.cpp)
target_link_libraries(test_widget_registry PRIVATE flexUI)
flexui_configure_tinytest(test_widget_registry)
target_include_directories(test_widget_registry PRIVATE ../include)
set_target_properties(test_widget_registry PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_widget_registry COMMAND test_widget_registry)

add_executable(test_script_controller test_script_controller.cpp)
target_link_libraries(test_script_controller PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_script_controller)
set_target_properties(test_script_controller PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_script_controller COMMAND test_script_controller)

if(FLEXUI_ENABLE_TURBOSCRIPT)
  add_executable(test_turboscript_controller test_turboscript_controller.cpp)
  target_link_libraries(
    test_turboscript_controller PRIVATE FlexUI::ControllerTurboScript)
  flexui_configure_tinytest(test_turboscript_controller)
  set_target_properties(
    test_turboscript_controller PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_turboscript_controller COMMAND test_turboscript_controller)
  if(WIN32)
    get_filename_component(
      _flexui_compiler_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_property(
      TEST test_turboscript_controller PROPERTY ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${_flexui_compiler_runtime_dir}")
  endif()
endif()

add_executable(test_ui_mutation test_ui_mutation.cpp)
target_link_libraries(test_ui_mutation PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_ui_mutation)
set_target_properties(test_ui_mutation PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_ui_mutation COMMAND test_ui_mutation)

add_executable(test_ui_handle test_ui_handle.cpp)
target_link_libraries(test_ui_handle PRIVATE flexUI)
flexui_configure_tinytest(test_ui_handle)
target_include_directories(test_ui_handle PRIVATE ../include)
set_target_properties(test_ui_handle PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_ui_handle COMMAND test_ui_handle)

add_executable(test_box_mutation_host test_box_mutation_host.cpp)
target_link_libraries(test_box_mutation_host PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_box_mutation_host)
set_target_properties(test_box_mutation_host PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_box_mutation_host COMMAND test_box_mutation_host)

add_executable(test_application_command test_application_command.cpp)
target_link_libraries(test_application_command PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_application_command)
set_target_properties(test_application_command PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_application_command COMMAND test_application_command)

add_executable(test_application_completion test_application_completion.cpp)
target_link_libraries(test_application_completion PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_application_completion)
set_target_properties(test_application_completion PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_application_completion COMMAND test_application_completion)

add_executable(test_application_service_requests
               test_application_service_requests.cpp)
target_link_libraries(test_application_service_requests PRIVATE FlexUI::Controller)
target_include_directories(
  test_application_service_requests
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../modules/controller")
flexui_configure_tinytest(test_application_service_requests)
set_target_properties(
  test_application_service_requests PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_application_service_requests
         COMMAND test_application_service_requests)

add_executable(test_application_service_dispatcher
               test_application_service_dispatcher.cpp)
target_link_libraries(test_application_service_dispatcher
                      PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_application_service_dispatcher)
set_target_properties(
  test_application_service_dispatcher PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_application_service_dispatcher
         COMMAND test_application_service_dispatcher)

add_executable(test_service_registry test_service_registry.cpp)
target_link_libraries(test_service_registry PRIVATE FlexUI::Services)
flexui_configure_tinytest(test_service_registry)
set_target_properties(test_service_registry PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_service_registry COMMAND test_service_registry)

add_executable(test_desktop_application test_desktop_application.cpp)
target_link_libraries(test_desktop_application PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_desktop_application)
set_target_properties(test_desktop_application PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_desktop_application COMMAND test_desktop_application)

add_executable(test_gcanvas_input test_gcanvas_input.cpp)
target_link_libraries(test_gcanvas_input PRIVATE FlexUI::GCanvasInput)
flexui_configure_tinytest(test_gcanvas_input)
set_target_properties(test_gcanvas_input PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_gcanvas_input COMMAND test_gcanvas_input)

add_executable(test_gcanvas_application_input test_gcanvas_application_input.cpp)
target_link_libraries(
  test_gcanvas_application_input
  PRIVATE FlexUI::GCanvasApplicationInput)
target_include_directories(
  test_gcanvas_application_input PRIVATE "${CMAKE_SOURCE_DIR}/shared/gcanvas/src")
flexui_configure_tinytest(test_gcanvas_application_input)
set_target_properties(
  test_gcanvas_application_input PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_gcanvas_application_input COMMAND test_gcanvas_application_input)

if(FLEX_BUILD_GCANVAS_GPU_TESTS AND TARGET FlexUI::GCanvasWindowHost)
  add_executable(test_gcanvas_window_host test_gcanvas_window_host.cpp)
  target_link_libraries(
    test_gcanvas_window_host PRIVATE FlexUI::GCanvasWindowHost)
  flexui_configure_tinytest(test_gcanvas_window_host)
  set_target_properties(
    test_gcanvas_window_host PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_gcanvas_window_host COMMAND test_gcanvas_window_host)

  if(FLEXUI_ENABLE_PLUGINS)
    add_executable(test_gcanvas_plugin_window_host
                   test_gcanvas_plugin_window_host.cpp)
    target_link_libraries(
      test_gcanvas_plugin_window_host
      PRIVATE FlexUI::GCanvasPluginWindowHost)
    flexui_configure_tinytest(test_gcanvas_plugin_window_host)
    target_compile_definitions(
      test_gcanvas_plugin_window_host
      PRIVATE FLEXUI_TEST_ECHO_PLUGIN="$<TARGET_FILE:flexui_test_plugin_echo>")
    add_dependencies(test_gcanvas_plugin_window_host flexui_test_plugin_echo)
    set_target_properties(
      test_gcanvas_plugin_window_host PROPERTIES FOLDER "flexUI/tests")
    add_test(NAME test_gcanvas_plugin_window_host
             COMMAND test_gcanvas_plugin_window_host)

    if(TARGET flexui_desktop_editor)
      add_test(NAME test_flexui_desktop_editor_smoke
               COMMAND flexui_desktop_editor --smoke)
      set_tests_properties(
        test_flexui_desktop_editor_smoke
        PROPERTIES LABELS "flexui;integration;gpu;turboscript;plugin")
    endif()
  endif()
endif()

if(FLEXUI_ENABLE_TURBOSCRIPT)
  add_executable(test_turboscript_application test_turboscript_application.cpp)
  target_link_libraries(
    test_turboscript_application PRIVATE FlexUI::ControllerTurboScript)
  flexui_configure_tinytest(test_turboscript_application)
  set_target_properties(
    test_turboscript_application PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_turboscript_application COMMAND test_turboscript_application)

  add_executable(test_desktop_editor_controller
                 test_desktop_editor_controller.cpp)
  target_link_libraries(
    test_desktop_editor_controller
    PRIVATE FlexUI::ControllerTurboScript Salts::Core)
  target_compile_definitions(
    test_desktop_editor_controller
    PRIVATE
      FLEXUI_EDITOR_CONTROLLER_PATH="${CMAKE_SOURCE_DIR}/flexUI/examples/desktop_editor/editor.tbs")
  flexui_configure_tinytest(test_desktop_editor_controller)
  set_target_properties(
    test_desktop_editor_controller PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_desktop_editor_controller
           COMMAND test_desktop_editor_controller)
  if(WIN32)
    set_property(
      TEST test_turboscript_application test_desktop_editor_controller
      PROPERTY ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${_flexui_compiler_runtime_dir}")
  endif()
endif()

add_executable(test_keyed_repeater test_keyed_repeater.cpp)
target_link_libraries(test_keyed_repeater PRIVATE flexUI)
flexui_configure_tinytest(test_keyed_repeater)
target_include_directories(test_keyed_repeater PRIVATE ../include)
set_target_properties(test_keyed_repeater PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_keyed_repeater COMMAND test_keyed_repeater)

if(WIN32)
  get_filename_component(
    _flexui_test_compiler_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(_flexui_runtime_tests
      test_plugin_abi
      test_application_service_requests
      test_service_registry)
  if(FLEXUI_ENABLE_PLUGINS)
    list(APPEND _flexui_runtime_tests test_plugin_host test_plugin_create_unwind
                test_plugin_manifest)
    if(TARGET test_gcanvas_plugin_window_host)
      list(APPEND _flexui_runtime_tests test_gcanvas_plugin_window_host)
    endif()
  endif()
  set_property(
    TEST ${_flexui_runtime_tests}
    PROPERTY ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:${_flexui_test_compiler_runtime_dir}"
             "PATH=path_list_prepend:${SALTS_ROOT}/${CMAKE_INSTALL_BINDIR}"
)

  if(TEST test_flexui_desktop_editor_smoke)
    set_property(
      TEST test_flexui_desktop_editor_smoke
      PROPERTY ENVIRONMENT_MODIFICATION
               "PATH=path_list_prepend:${_flexui_test_compiler_runtime_dir}"
               "PATH=path_list_prepend:${SALTS_ROOT}/${CMAKE_INSTALL_BINDIR}"
)
  endif()
endif()

add_executable(test_ui_content_models test_ui_content_models.cpp)
target_link_libraries(test_ui_content_models PRIVATE flexUI Salts::TinyTest)
add_test(NAME test_ui_content_models COMMAND test_ui_content_models)
