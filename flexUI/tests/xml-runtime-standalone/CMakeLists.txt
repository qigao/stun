cmake_minimum_required(VERSION 3.20...3.31)
project(StunXmlRuntimeQualification LANGUAGES C CXX)
include(CTest)
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_package(SaltsUtils CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_package(Threads REQUIRED)
find_program(RE2C_EXECUTABLE re2c REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ENABLE_SANITIZER_ADDRESS ON CACHE BOOL "Qualify production runtime")
set(ENABLE_SANITIZER_UNDEFINED ON CACHE BOOL "Qualify production runtime")
include("${STUN_ROOT}/cmake/Sanitizers.cmake")

# The same production targets and complete source lists used by the application.
# Only their optional test/example/window/GPU/JIT consumers are disabled.
set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
set(NANOGUI_BUILD_EXAMPLES OFF)
set(FLEXUI_ENABLE_PLUGINS ON)
set(FLEXUI_ENABLE_TURBOSCRIPT OFF)
foreach(option IN ITEMS GCANVAS_BUILD_OPENGL GCANVAS_BUILD_GLES
    GCANVAS_BUILD_ANDROID_EGL GCANVAS_BUILD_VULKAN GCANVAS_BUILD_SVG
    GCANVAS_BUILD_WINDOW GCANVAS_BUILD_TESTS GCANVAS_BUILD_EXAMPLES
    FLEX_BUILD_RENDER_ENGINE_GCANVAS FLEX_BUILD_RENDER_ENGINE_NANOVG
    FLEX_BUILD_BACKEND_OPENGL FLEX_BUILD_BACKEND_VULKAN FLEX_BUILD_BACKEND_D2D
    FLEX_BUILD_BINARY FLEX_BUILD_TOOLS)
  set(${option} OFF CACHE BOOL "Not part of headless XML qualification" FORCE)
endforeach()
set(FLEX_BUILD_BACKEND_TUI ON CACHE BOOL "Headless real Tango renderer integration" FORCE)
# These production modules still use repository-root include paths.
include_directories("${STUN_ROOT}/vendor/tinyfsm" "${STUN_ROOT}/vendor/mir"
  "${STUN_ROOT}/vendor/miniblas" "${STUN_ROOT}/tools/tree-sitter/plugins")
add_executable(stun_xml_lemon "${STUN_ROOT}/tools/lemon/lemon.c")
set(LEMON_EXECUTABLE "$<TARGET_FILE:stun_xml_lemon>")
set(LEMON_DEPENDS stun_xml_lemon)
set(LEMPAR "${STUN_ROOT}/tools/lemon/lempar.c")
add_subdirectory("${STUN_ROOT}/vendor/mir" mir)
add_subdirectory("${STUN_ROOT}/vendor/miniblas" miniblas)
add_subdirectory("${STUN_ROOT}/shared/gcanvas" gcanvas)
add_subdirectory("${STUN_ROOT}/shared/markdown" markdown)
add_subdirectory("${STUN_ROOT}/tango" tango)
add_subdirectory("${STUN_ROOT}/flex" flex)
add_subdirectory("${STUN_ROOT}/flexUI" flexUI)
target_link_libraries(flexUI PRIVATE Stun::UICore)
# Link the SAME native graph placement and route algorithms used by the
# production DotGraphRenderer; configure these targets while BUILD_TESTING
# is OFF to avoid unrelated GraphLayout test duplication in this focused gate.
add_subdirectory("${STUN_ROOT}/shared/layout" graphlayout)

set(BUILD_TESTING ON)
add_custom_target(stun_xml_runtime_qualification)

# Reuse the production compiler targets and style bridge. Keeping these tests
# independent of FlexUI::Core makes accidental CSS/runtime dependencies visible.
add_executable(test_tailwind_native
  "${STUN_ROOT}/flexUI/tests/test_tailwind_native.cpp")
target_link_libraries(test_tailwind_native
  PRIVATE FlexUI::TailwindNative Salts::TinyTest)
add_executable(test_tailwind_native_style
  "${STUN_ROOT}/flexUI/tests/test_tailwind_native_style.cpp"
  "${STUN_ROOT}/flexUI/src/tailwind_style_apply.cpp"
  "${STUN_ROOT}/flexUI/src/style_property_registry.cpp")
target_link_libraries(test_tailwind_native_style
  PRIVATE FlexUI::TailwindNative Salts::Core Salts::CMeta Salts::TinyTest)
add_executable(test_tailwindcss_module
  "${STUN_ROOT}/flexUI/tests/test_tailwindcss_module.cpp")
target_link_libraries(test_tailwindcss_module
  PRIVATE FlexUI::TailwindCSS Salts::TinyTest)
foreach(test IN ITEMS test_tailwind_native test_tailwind_native_style
                      test_tailwindcss_module)
  add_test(NAME ${test} COMMAND ${test})
  set_tests_properties(${test} PROPERTIES TIMEOUT 120)
  add_dependencies(stun_xml_runtime_qualification ${test})
endforeach()

foreach(test IN ITEMS test_ui_content_models test_ui_xml_admission test_ui_xml
    test_ui_document test_widget_registry test_desktop_application
    test_script_controller test_application_service_requests
    test_service_registry test_application_completion
    test_application_service_dispatcher)
  add_executable(${test} "${STUN_ROOT}/flexUI/tests/${test}.cpp")
  target_link_libraries(${test} PRIVATE FlexUI::Controller Salts::TinyTest)
  add_test(NAME ${test} COMMAND ${test})
  set_tests_properties(${test} PROPERTIES TIMEOUT 120)
  add_dependencies(stun_xml_runtime_qualification ${test})
endforeach()
# This exact production Controller/service boundary now includes the registry,
# bounded MPSC mailbox, and owner-thread dispatcher; all run with ASan/UBSan
# and a separate LSan build, plus Windows/macOS headless native CTest.
# C11 PluginSDK remains installable without plugin runtime dependencies.
# Require the ONE current ABI v2 header, its exact entry-point spelling and
# the actual PluginModule bridge source to compile on all native CI hosts.
add_executable(test_plugin_abi "${STUN_ROOT}/flexUI/tests/test_plugin_abi.c")
target_link_libraries(test_plugin_abi PRIVATE FlexUI::PluginSDK Salts::TinyTest)
set_target_properties(test_plugin_abi PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
add_test(NAME test_plugin_abi COMMAND test_plugin_abi)
set_tests_properties(test_plugin_abi PROPERTIES TIMEOUT 120)
add_dependencies(stun_xml_runtime_qualification test_plugin_abi)

# ABI v2 is now qualified through the actual dynamically loaded PluginHost,
# provider fixtures, worker completion protocol and stop/join tests rather
# than by compiling isolated object files and guessing runtime behavior.
include("${STUN_ROOT}/flexUI/tests/plugin-native-targets.cmake")
flexui_add_native_plugin_tests()
foreach(test IN ITEMS test_plugin_host test_plugin_manifest test_plugin_create_unwind)
  set_tests_properties(${test} PROPERTIES TIMEOUT 120)
  add_dependencies(stun_xml_runtime_qualification ${test})
endforeach()

# Exercise the real private service queue against published FlexUI providers.
target_include_directories(test_application_service_requests PRIVATE
    "${STUN_ROOT}/flexUI/modules/controller")

# Compile the EXACT optional production WindowHost source on all three native
# headless platforms, even when GPU / GLFW presentation targets are disabled.
# This is a compile-only source/ABI gate, not a GPU/window runtime acceptance.
# Do not create mocks or a duplicate host implementation.
add_library(stun_gcanvas_window_host_source_compile OBJECT
    "${STUN_ROOT}/flexUI/modules/desktop/gcanvas_window_host.cpp")
target_link_libraries(stun_gcanvas_window_host_source_compile PRIVATE
    FlexUI::GCanvasApplicationInput)
target_include_directories(stun_gcanvas_window_host_source_compile PRIVATE
    "${STUN_ROOT}/flex/render/engines/gcanvas/include")
add_dependencies(stun_xml_runtime_qualification
    stun_gcanvas_window_host_source_compile)

# Reuse the production ChartFrame XML/component adapter and its real
# TinyTest/snapshot suite in the installed-SDK ASan/UBSan and LSan gate.
# Avoid a chart-specific implementation or mock Box in qualification.
add_subdirectory("${STUN_ROOT}/charts/ui" chart_ui)
add_dependencies(stun_xml_runtime_qualification chart_ui_test)

# Production DOT parser (re2c + lemon), GraphLayout-backed renderer, source
# facade and native XML snapshot builder are linked in this exact same target.
# This is an end-to-end source→AST→layout→UiDocument→Widget acceptance, not
# a new parser, mock box, duplicate algorithm or standalone fake renderer.
add_subdirectory("${STUN_ROOT}/charts/dotgraph" dotgraph)
add_dependencies(stun_xml_runtime_qualification dotgraph_parser_test)


# Compile the real production FlexChart→XML adapter against published SDKs,
# even though parser/mark DSL test assets are deliberately not linked into
# this minimal headless runtime. ChartUI's native binding/rollback suites
# execute separately in ChartUiTest; this target checks the actual consumer.
add_library(stun_flexchart_ui_adapter_compile OBJECT
    "${STUN_ROOT}/charts/flexchart/src/flexui_chart.cpp")
target_include_directories(stun_flexchart_ui_adapter_compile PRIVATE
    "${STUN_ROOT}/charts/flexchart/include"
    "${STUN_ROOT}/charts/flexchart/src")
target_link_libraries(stun_flexchart_ui_adapter_compile PRIVATE chart_ui flex)
add_dependencies(stun_xml_runtime_qualification
    stun_flexchart_ui_adapter_compile)
add_dependencies(chart_ui_test stun_flexchart_ui_adapter_compile)

# Compile the actual Infographic facade through the same released-SDK
# contract, not only a synthetic Plot Widget. Full infographic layout/IR
# parser tests remain separately owned by charts/infographic.
add_library(stun_infographic_ui_adapter_compile OBJECT
    "${STUN_ROOT}/charts/infographic/src/flexui_infographic.cpp")
target_include_directories(stun_infographic_ui_adapter_compile PRIVATE
    "${STUN_ROOT}/charts/infographic/include/infographic")
target_link_libraries(stun_infographic_ui_adapter_compile PRIVATE chart_ui flex)
add_dependencies(stun_xml_runtime_qualification
    stun_infographic_ui_adapter_compile)
add_dependencies(chart_ui_test stun_infographic_ui_adapter_compile)

# The snapshot→XML widget test now links the production flex_dotgraph_adapter.
# It directly exercises typed geometry, real widget rendering and rollback;
# DotGraphParserTest separately exercises the entire DOT text/parser/layout
# path through that SAME adapter and its registered CSS/semantic Elements.
add_executable(test_dotgraph_ui_xml
    "${STUN_ROOT}/charts/dotgraph/tests/test_dotgraph_ui_xml.cpp")
target_link_libraries(test_dotgraph_ui_xml PRIVATE
    flex_dotgraph_adapter chart_ui flex Salts::TinyTest)
add_test(NAME DotGraphXmlTest COMMAND test_dotgraph_ui_xml)
set_tests_properties(DotGraphXmlTest PROPERTIES TIMEOUT 120)
add_dependencies(stun_xml_runtime_qualification test_dotgraph_ui_xml)

# The MarkdownWidget streaming consumer contract must run in the focused
# headless production-runtime gate; parser conformance alone does not exercise
# the FlexUI projection/lifecycle path.
add_executable(test_markdown_widget "${STUN_ROOT}/flexUI/tests/test_markdown_widget.cpp")
target_link_libraries(test_markdown_widget PRIVATE flexUI Salts::TinyTest)
add_test(NAME test_markdown_widget COMMAND test_markdown_widget)
set_tests_properties(test_markdown_widget PROPERTIES TIMEOUT 120)
add_dependencies(stun_xml_runtime_qualification test_markdown_widget)

# The same installed-SDK gate executes a real Tango mailbox, FlexUI Markdown
# projection, and the actual Flex→Tango cell renderer with no host terminal.
add_executable(test_flexui_tango_stream
  "${STUN_ROOT}/flexUI/tests/test_flexui_tango_stream.cpp")
target_link_libraries(test_flexui_tango_stream PRIVATE
  flexUI flex_backend_tui tango Salts::TinyTest Threads::Threads)
# Deliberately test the existing private platform wait entry, not a new
# public or duplicated event-loop abstraction. The installed C core remains
# independent of FlexUI/GUI/C++.
target_include_directories(test_flexui_tango_stream PRIVATE
  "${STUN_ROOT}/tango/src")
add_test(NAME test_flexui_tango_stream COMMAND test_flexui_tango_stream)
set_tests_properties(test_flexui_tango_stream PROPERTIES TIMEOUT 120)
add_dependencies(stun_xml_runtime_qualification test_flexui_tango_stream)

# Build and actually run the same production Demo UI callbacks with a
# synthetic terminal in CI. This does not replace physical terminal testing.
option(STUN_BUILD_TUI_STREAMING_DEMO
       "Build/test real Tango + FlexUI Streaming sample" ON)
if(STUN_BUILD_TUI_STREAMING_DEMO)
  add_executable(flexui_tui_streaming_demo
    "${STUN_ROOT}/flexUI/examples/tui_streaming_demo.cpp")
  target_link_libraries(flexui_tui_streaming_demo PRIVATE
    flexUI flex_backend_tui tango Threads::Threads)
  target_compile_definitions(flexui_tui_streaming_demo PRIVATE
    STUN_TUI_DEMO_HEADLESS_TEST=1)
  target_include_directories(flexui_tui_streaming_demo PRIVATE
    "${STUN_ROOT}/tango/src")
  # Strictly CI-only: allow the real Tango C11 render/flush path to attest
  # to a post-write committed frame. Ordinary runtime targets stay unchanged.
  if(UNIX)
    target_compile_definitions(tango PRIVATE STUN_TUI_PTY_RENDER_TRACE_CI=1)
    target_compile_definitions(flexui_tui_streaming_demo PRIVATE
      STUN_TUI_PTY_RENDER_TRACE_CI=1)
  endif()
  # No mode switch: the default smoke MUST exercise bounded coordinate
  # virtual scrolling with live Box height measurement and scroll anchors.
  add_test(NAME tui_streaming_demo_smoke
    COMMAND flexui_tui_streaming_demo --headless-smoke)
  set_tests_properties(tui_streaming_demo_smoke PROPERTIES TIMEOUT 90)
  # Compatibility alias for applications still passing the old mode flag.
  # Same 80x24 synthetic-owner/front-buffer fixture with virtual geometry.
  # In particular, 80x24 equals the provisional Box size: the FIRST owner
  # turn must lay out root/viewport before height-index sync is admitted.
  add_test(NAME tui_streaming_demo_smoke_virtual_spacers
    COMMAND flexui_tui_streaming_demo --headless-smoke
      --experimental-virtual-spacers)
  set_tests_properties(tui_streaming_demo_smoke_virtual_spacers
    PROPERTIES TIMEOUT 90)
  # An explicit opt-out must preserve the old fixed-host message-step mode.
  add_test(NAME tui_streaming_demo_smoke_legacy
    COMMAND flexui_tui_streaming_demo --headless-smoke
      --legacy-message-scroll)
  set_tests_properties(tui_streaming_demo_smoke_legacy
    PROPERTIES TIMEOUT 90)
  add_dependencies(stun_xml_runtime_qualification flexui_tui_streaming_demo)

  # The separate native POSIX PTY gate runs the *ordinary* interactive
  # tui_app_run() branch: actual termios, kernel tty input, SIGWINCH and
  # alternate-screen/keyboard restoration. The synthetic smoke above does
  # not substitute for this test. Run on POSIX Linux and macOS; Windows
  # keeps the independent synthetic TUI + native Console acceptance gates.
  if(UNIX)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME tui_streaming_demo_posix_pty
      COMMAND "${Python3_EXECUTABLE}"
        "${STUN_ROOT}/flexUI/tests/test_tui_streaming_pty.py"
        "$<TARGET_FILE:flexui_tui_streaming_demo>")
    set_tests_properties(tui_streaming_demo_posix_pty
      PROPERTIES TIMEOUT 60)
    # Separate real PTY handshake: Home must be decoded, admitted and
    # screen-visible while the first worker BEGIN is still blocked on a
    # one-byte test-only pipe. The normal concurrent PTY gate above stays.
    add_test(NAME tui_streaming_demo_posix_pty_early_home
      COMMAND "${Python3_EXECUTABLE}"
        "${STUN_ROOT}/flexUI/tests/test_tui_streaming_pty.py"
        "$<TARGET_FILE:flexui_tui_streaming_demo>" --early-home)
    set_tests_properties(tui_streaming_demo_posix_pty_early_home
      PROPERTIES TIMEOUT 60)
    # Independent active-producer input-admission gate: drive Home/End
    # through real POSIX PTY, including fragmented Home CSI writes, while
    # Markdown is still streaming. Keep the original 5-second visible-
    # state deadlines and the normal PTY fixture untouched.
    add_test(NAME tui_streaming_demo_posix_pty_home_burst
      COMMAND "${Python3_EXECUTABLE}"
        "${STUN_ROOT}/flexUI/tests/test_tui_streaming_pty.py"
        "$<TARGET_FILE:flexui_tui_streaming_demo>" --home-burst)
    set_tests_properties(tui_streaming_demo_posix_pty_home_burst
      PROPERTIES TIMEOUT 60)
    # Explicit opt-out under a real PTY preserves the old message-step
    # browse behavior and the same strict screen/termios restoration gate.
    add_test(NAME tui_streaming_demo_posix_pty_legacy
      COMMAND "${Python3_EXECUTABLE}"
        "${STUN_ROOT}/flexUI/tests/test_tui_streaming_pty.py"
        "$<TARGET_FILE:flexui_tui_streaming_demo>" --legacy-message-scroll)
    set_tests_properties(tui_streaming_demo_posix_pty_legacy
      PROPERTIES TIMEOUT 60)
    # Additional stress of the *default* virtual mode with a CI-only rapid
    # producer: real OS PTY input, history eviction, native Spacer geometry,
    # SIGWINCH 80x24 -> 52x16, and terminal restoration.
    # The ordinary-rate default PTY / strict macOS Home gates remain.
    add_test(NAME tui_streaming_demo_posix_pty_virtual_spacers
      COMMAND "${Python3_EXECUTABLE}"
        "${STUN_ROOT}/flexUI/tests/test_tui_streaming_pty.py"
        "$<TARGET_FILE:flexui_tui_streaming_demo>" --virtual-spacers)
    set_tests_properties(tui_streaming_demo_posix_pty_virtual_spacers
      PROPERTIES TIMEOUT 60)
  elseif(WIN32)
    # Spawn the unchanged real interactive demo under a fresh OS Console.
    # Independent supervising process writes INPUT_RECORDs and reads native
    # screen cells; never call event_map or the private owner callbacks.
    add_executable(test_tui_win32_console
      "${STUN_ROOT}/flexUI/tests/test_tui_win32_console.cpp")
    add_test(NAME tui_streaming_demo_win32_console
      COMMAND test_tui_win32_console "$<TARGET_FILE:flexui_tui_streaming_demo>")
    set_tests_properties(tui_streaming_demo_win32_console
      PROPERTIES TIMEOUT 80)
    add_dependencies(stun_xml_runtime_qualification test_tui_win32_console)

    # Host-owned resize is distinct from native Console INPUT_RECORD.
    # ResizePseudoConsole changes the actual child console dimensions;
    # the separate supervisor verifies the unchanged demo's visible cells.
    add_executable(test_tui_win32_conpty
      "${STUN_ROOT}/flexUI/tests/test_tui_win32_conpty.cpp")
    add_test(NAME tui_streaming_demo_win32_conpty
      COMMAND test_tui_win32_conpty "$<TARGET_FILE:flexui_tui_streaming_demo>")
    set_tests_properties(tui_streaming_demo_win32_conpty
      PROPERTIES TIMEOUT 85)
    # The instrumented route is diagnostic-only: the ordinary demo without
    # a report file is also a required native test. Never let owner-side
    # logging make an otherwise failing race appear qualified.
    add_test(NAME tui_streaming_demo_win32_conpty_plain
      COMMAND test_tui_win32_conpty --plain
        "$<TARGET_FILE:flexui_tui_streaming_demo>")
    set_tests_properties(tui_streaming_demo_win32_conpty_plain
      PROPERTIES TIMEOUT 85)
    # Compatibility alias on the default virtual path; both the ordinary
    # no-argument and alias tests use the same real ConPTY host/resizer.
    # No synthetic console replaces the default plain/native tests.
    add_test(NAME tui_streaming_demo_win32_conpty_virtual_spacers
      COMMAND test_tui_win32_conpty --virtual-spacers
        "$<TARGET_FILE:flexui_tui_streaming_demo>")
    set_tests_properties(tui_streaming_demo_win32_conpty_virtual_spacers
      PROPERTIES TIMEOUT 85)
    add_test(NAME tui_streaming_demo_win32_conpty_legacy
      COMMAND test_tui_win32_conpty --legacy-message-scroll
        "$<TARGET_FILE:flexui_tui_streaming_demo>")
    set_tests_properties(tui_streaming_demo_win32_conpty_legacy
      PROPERTIES TIMEOUT 85)
    add_dependencies(stun_xml_runtime_qualification test_tui_win32_conpty)
  endif()
endif()

if(TARGET gCanvas::Window OR TARGET flex_render_engine_gcanvas OR
   TARGET FlexUI::ControllerTurboScript)
  message(FATAL_ERROR "XML runtime gate must remain headless and independent of TurboScript JIT")
endif()
