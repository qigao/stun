# Sole source of the real plugin DSO fixtures and native runtime tests.
# Both normal repository tests and exact-head installed-SDK native qualification
# invoke this graph; do not invent a second plugin host or fixture provider.
set(FLEXUI_PLUGIN_NATIVE_TESTS_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(FLEXUI_PLUGIN_PROJECT_ROOT
    "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/.." ABSOLUTE)

function(flexui_add_native_plugin_tests)
  if(NOT TARGET FlexUI::PluginHost)
    message(FATAL_ERROR "PluginHost production target required for native plugin acceptance")
  endif()
  add_library(flexui_test_plugin_missing_entry MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_missing_entry.c")
  add_library(flexui_test_plugin_bad_abi MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_bad_abi.c")
  add_library(flexui_test_plugin_old_minor MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_bad_abi.c")
  target_compile_definitions(flexui_test_plugin_old_minor
    PRIVATE FLEXUI_TEST_OLD_MINOR=1)
  add_library(flexui_test_plugin_echo MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_fail_start MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_fail_create MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_fail_stop MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_static_crt MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_provider MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_consumer MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_echo.cpp")
  add_library(flexui_test_plugin_create_unwind MODULE "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/plugin_test_create_unwind.cpp")
  target_link_libraries(
    flexui_test_plugin_create_unwind PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_bad_abi PRIVATE FlexUI::PluginSDK)
  target_link_libraries(
    flexui_test_plugin_old_minor PRIVATE FlexUI::PluginSDK)
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
    flexui_test_plugin_old_minor
    flexui_test_plugin_fail_create flexui_test_plugin_fail_stop
    flexui_test_plugin_static_crt
    flexui_test_plugin_provider flexui_test_plugin_consumer
    flexui_test_plugin_create_unwind
    PROPERTIES FOLDER "flexUI/tests/plugins"
               C_VISIBILITY_PRESET hidden
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON)

  add_executable(test_plugin_host "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/test_plugin_host.cpp")
  target_link_libraries(test_plugin_host PRIVATE FlexUI::PluginHost FlexUI::Controller)
  target_link_libraries(test_plugin_host PRIVATE Salts::TinyTest)
  target_compile_definitions(
    test_plugin_host
    PRIVATE FLEXUI_TEST_ECHO_PLUGIN="$<TARGET_FILE:flexui_test_plugin_echo>"
            FLEXUI_TEST_BAD_ABI_PLUGIN="$<TARGET_FILE:flexui_test_plugin_bad_abi>"
            FLEXUI_TEST_OLD_MINOR_PLUGIN="$<TARGET_FILE:flexui_test_plugin_old_minor>"
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
    flexui_test_plugin_old_minor
    flexui_test_plugin_missing_entry)
  set_target_properties(test_plugin_host PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_plugin_host COMMAND test_plugin_host)

  add_executable(test_plugin_create_unwind "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/test_plugin_create_unwind.cpp")
  target_link_libraries(test_plugin_create_unwind PRIVATE FlexUI::PluginHost)
  target_link_libraries(test_plugin_create_unwind PRIVATE Salts::TinyTest)
  target_include_directories(
    test_plugin_create_unwind PRIVATE ${FLEXUI_PLUGIN_PROJECT_ROOT}/modules/plugin_host)
  target_compile_definitions(
    test_plugin_create_unwind
    PRIVATE FLEXUI_TEST_CREATE_UNWIND_PLUGIN="$<TARGET_FILE:flexui_test_plugin_create_unwind>")
  add_dependencies(test_plugin_create_unwind flexui_test_plugin_create_unwind)
  set_target_properties(test_plugin_create_unwind PROPERTIES FOLDER "flexUI/tests")
  add_test(NAME test_plugin_create_unwind COMMAND test_plugin_create_unwind)

  add_executable(test_plugin_manifest "${FLEXUI_PLUGIN_NATIVE_TESTS_DIR}/test_plugin_manifest.cpp")
  target_link_libraries(test_plugin_manifest PRIVATE FlexUI::PluginHost)
  target_include_directories(
    test_plugin_manifest PRIVATE ${FLEXUI_PLUGIN_PROJECT_ROOT}/modules/plugin_host)
  target_link_libraries(test_plugin_manifest PRIVATE Salts::TinyTest)
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
endfunction()
