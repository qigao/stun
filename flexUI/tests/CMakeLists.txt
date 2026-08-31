enable_testing()

find_package(nlohmann_json CONFIG REQUIRED)

function(flexui_configure_tinytest target_name)
  target_link_libraries(${target_name} PRIVATE TurboUtils::TinyTest)
endfunction()

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

add_executable(test_script_controller test_script_controller.cpp)
target_link_libraries(test_script_controller PRIVATE FlexUI::Controller)
flexui_configure_tinytest(test_script_controller)
set_target_properties(test_script_controller PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_script_controller COMMAND test_script_controller)

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

add_executable(test_keyed_repeater test_keyed_repeater.cpp)
target_link_libraries(test_keyed_repeater PRIVATE flexUI)
flexui_configure_tinytest(test_keyed_repeater)
target_include_directories(test_keyed_repeater PRIVATE ../include)
set_target_properties(test_keyed_repeater PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_keyed_repeater COMMAND test_keyed_repeater)
