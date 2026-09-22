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
  add_library(flexui_test_plugin_missing_entry MODULE plugin_test_missing_entry.c)
  add_library(flexui_test_plugin_bad_abi MODULE plugin_test_bad_abi.c)
  add_library(flexui_test_plugin_echo MODULE plugin_test_echo.cpp)
  add_library(flexui_test_plugin_fail_start MODULE plugin_test_echo.cpp)
  add_library(flexui_test_plugin_fail_create MODULE plugin_test_echo.cpp)
  add_library(flexui_test_plugin_fail_stop MODULE plugin_test_echo.cpp)
  add_library(flexui_test_plugin_static_crt MODULE plugin_test_echo.cpp)
  add_library(flexui_test_plugin_provider MODULE plugin_test_echo.cpp)
  add_library(flexui_test_plugin_consumer MODULE plugin_test_echo.cpp)
  target_link_libraries(
    flexui_test_plugin_bad_abi PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_echo PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_fail_start PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_fail_create PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_fail_stop PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_static_crt PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_provider PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_consumer PRIVATE FlexUI::PluginSDK)
  target_compile_definitions(
    flexui_test_plugin_fail_start PRIVATE FLEXUI_TEST_FAIL_START=1)
  target_compile_definitions(
    flexui_test_plugin_fail_create
    PRIVATE FLEXUI_TEST_FAIL_CREATE=1
            FLEXUI_TEST_PLUGIN_ID="test.fail-create"
            FLEXUI_TEST_PLUGIN_SERVICE="test.fail-create/1")
  target_compile_definitions(
    flexui_test_plugin_fail_stop
    PRIVATE FLEXUI_TEST_FAIL_STOP_ONCE=1
            FLEXUI_TEST_PLUGIN_ID="test.fail-stop"
            FLEXUI_TEST_PLUGIN_SERVICE="test.fail-stop/1")
  target_compile_definitions(
    flexui_test_plugin_static_crt
    PRIVATE FLEXUI_TEST_PLUGIN_ID="test.static-crt"
            FLEXUI_TEST_PLUGIN_SERVICE="test.static-crt/1")
  if(MSVC)
    set_property(
      TARGET flexui_test_plugin_static_crt
      PROPERTY MSVC_RUNTIME_LIBRARY
               "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  target_compile_definitions(
    flexui_test_plugin_provider
    PRIVATE FLEXUI_TEST_PLUGIN_ID="test.provider"
            FLEXUI_TEST_PLUGIN_SERVICE="test.provider/1")
  target_compile_definitions(
    flexui_test_plugin_consumer
    PRIVATE FLEXUI_TEST_PLUGIN_ID="test.consumer"
            FLEXUI_TEST_PLUGIN_SERVICE="test.consumer/1")
  set_target_properties(
    flexui_test_plugin_missing_entry flexui_test_plugin_bad_abi
    flexui_test_plugin_echo flexui_test_plugin_fail_start
    flexui_test_plugin_fail_create flexui_test_plugin_fail_stop
    flexui_test_plugin_static_crt
    flexui_test_plugin_provider flexui_test_plugin_consumer
    PROPERTIES FOLDER "flexUI/tests/plugins"
               C_VISIBILITY_PRESET hidden
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON)

  add_executable(test_plugin_host test_plugin_host.cpp)
  target_link_libraries(test_plugin_host PRIVATE FlexUI::PluginHost)
  flexui_configure_tinytest(test_plugin_host)
  target_compile_definitions(
    test_plugin_host
    PRIVATE FLEXUI_TEST_ECHO_PLUGIN="$<TARGET_FILE:flexui_test_plugin_echo>"
            FLEXUI_TEST_BAD_ABI_PLUGIN="$<TARGET_FILE:flexui_test_plugin_bad_abi>"
            FLEXUI_TEST_MISSING_ENTRY_PLUGIN="$<TARGET_FILE:flexui_test_plugin_missing_entry>"
            FLEXUI_TEST_FAIL_START_PLUGIN="$<TARGET_FILE:flexui_test_plugin_fail_start>"
            FLEXUI_TEST_FAIL_CREATE_PLUGIN="$<TARGET_FILE:flexui_test_plugin_fail_create>"
            FLEXUI_TEST_FAIL_STOP_PLUGIN="$<TARGET_FILE:flexui_test_plugin_fail_stop>"
            FLEXUI_TEST_STATIC_CRT_PLUGIN="$<TARGET_FILE:flexui_test_plugin_static_crt>")
  add_dependencies(
    test_plugin_host flexui_test_plugin_echo flexui_test_plugin_fail_start
    flexui_test_plugin_fail_create flexui_test_plugin_fail_stop
    flexui_test_plugin_static_crt
    flexui_test_plugin_bad_abi
    flexui_test_plugin_missing_entry)
  set_target_properties(test_plugin_host PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_plugin_host COMMAND test_plugin_host)

  add_executable(test_plugin_manifest test_plugin_manifest.cpp)
  target_link_libraries(test_plugin_manifest PRIVATE FlexUI::PluginHost)
  target_include_directories(
    test_plugin_manifest PRIVATE ${CMAKE_SOURCE_DIR}/flexUI/modules/plugin_host)
  flexui_configure_tinytest(test_plugin_manifest)
  target_compile_definitions(
    test_plugin_manifest
    PRIVATE FLEXUI_TEST_ECHO_PLUGIN="$<TARGET_FILE:flexui_test_plugin_echo>"
            FLEXUI_TEST_PROVIDER_PLUGIN="$<TARGET_FILE:flexui_test_plugin_provider>"
            FLEXUI_TEST_CONSUMER_PLUGIN="$<TARGET_FILE:flexui_test_plugin_consumer>")
  add_dependencies(
    test_plugin_manifest flexui_test_plugin_echo flexui_test_plugin_provider
    flexui_test_plugin_consumer)
  set_target_properties(test_plugin_manifest PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_plugin_manifest COMMAND test_plugin_manifest)
endif()

add_executable(test_markdown_widget test_markdown_widget.cpp)
target_link_libraries(test_markdown_widget PRIVATE flexUI)
flexui_configure_tinytest(test_markdown_widget)
target_include_directories(
  test_markdown_widget PRIVATE ../include ${CMAKE_SOURCE_DIR}/vendor/md4c/src)
set_target_properties(test_markdown_widget PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_markdown_widget COMMAND test_markdown_widget)

add_executable(test_style_engine test_style_engine.cpp)
target_link_libraries(test_style_engine PRIVATE flexUI)
flexui_configure_tinytest(test_style_engine)
target_include_directories(test_style_engine PRIVATE ../include)
set_target_properties(test_style_engine PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_style_engine COMMAND test_style_engine)

add_executable(test_text_util_unicode test_text_util_unicode.cpp)
target_link_libraries(test_text_util_unicode PRIVATE flexUI)
flexui_configure_tinytest(test_text_util_unicode)
set_target_properties(test_text_util_unicode PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_text_util_unicode COMMAND test_text_util_unicode)

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
    set(_flexui_turbonet_runtime_hints
        "${TURBONET_ROOT}/bin"
        "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/tools/boringssl")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      list(PREPEND _flexui_turbonet_runtime_hints
           "${TURBONET_ROOT}/debug/bin")
    elseif(CMAKE_BUILD_TYPE)
      list(PREPEND _flexui_turbonet_runtime_hints
           "${TURBONET_ROOT}/release/bin")
    endif()
    find_file(
      _flexui_boringssl_ssl NAMES ssl.dll
      HINTS ${_flexui_turbonet_runtime_hints} NO_DEFAULT_PATH REQUIRED)
    find_file(
      _flexui_boringssl_crypto NAMES crypto.dll
      HINTS ${_flexui_turbonet_runtime_hints} NO_DEFAULT_PATH REQUIRED)
    get_filename_component(
      _flexui_boringssl_runtime_dir "${_flexui_boringssl_ssl}" DIRECTORY)
    get_filename_component(
      _flexui_boringssl_crypto_dir "${_flexui_boringssl_crypto}" DIRECTORY)
    if(NOT _flexui_boringssl_runtime_dir STREQUAL
       _flexui_boringssl_crypto_dir)
      message(FATAL_ERROR
              "FlexUI TurboScript tests require ssl.dll and crypto.dll from one runtime directory")
    endif()
    set_property(
      TEST test_turboscript_controller PROPERTY ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${_flexui_compiler_runtime_dir}"
      "PATH=path_list_prepend:${_flexui_boringssl_runtime_dir}")
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
  test_gcanvas_application_input PRIVATE "${gCanvas_SOURCE_DIR}/src")
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
      "PATH=path_list_prepend:${_flexui_compiler_runtime_dir}"
      "PATH=path_list_prepend:${_flexui_boringssl_runtime_dir}")
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
    list(APPEND _flexui_runtime_tests test_plugin_host test_plugin_manifest)
    if(TARGET test_gcanvas_plugin_window_host)
      list(APPEND _flexui_runtime_tests test_gcanvas_plugin_window_host)
    endif()
  endif()
  set_property(
    TEST ${_flexui_runtime_tests}
    PROPERTY ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:${_flexui_test_compiler_runtime_dir}"
             "PATH=path_list_prepend:${SALTS_ROOT}/${CMAKE_INSTALL_BINDIR}"
             "PATH=path_list_prepend:${TURBOPARSER_ROOT}/${CMAKE_INSTALL_BINDIR}")

  if(TEST test_flexui_desktop_editor_smoke)
    set_property(
      TEST test_flexui_desktop_editor_smoke
      PROPERTY ENVIRONMENT_MODIFICATION
               "PATH=path_list_prepend:${_flexui_test_compiler_runtime_dir}"
               "PATH=path_list_prepend:${_flexui_boringssl_runtime_dir}"
               "PATH=path_list_prepend:${SALTS_ROOT}/${CMAKE_INSTALL_BINDIR}"
               "PATH=path_list_prepend:${TURBOPARSER_ROOT}/${CMAKE_INSTALL_BINDIR}")
  endif()
endif()
