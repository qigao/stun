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
set(FLEXUI_ENABLE_PLUGINS OFF)
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

set(BUILD_TESTING ON)
add_custom_target(stun_xml_runtime_qualification)
foreach(test IN ITEMS test_ui_content_models test_ui_xml_admission test_ui_xml
    test_ui_document test_widget_registry test_desktop_application)
  add_executable(${test} "${STUN_ROOT}/flexUI/tests/${test}.cpp")
  target_link_libraries(${test} PRIVATE FlexUI::Controller Salts::TinyTest)
  add_test(NAME ${test} COMMAND ${test})
  set_tests_properties(${test} PROPERTIES TIMEOUT 120)
  add_dependencies(stun_xml_runtime_qualification ${test})
endforeach()

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
  add_test(NAME tui_streaming_demo_smoke
    COMMAND flexui_tui_streaming_demo --headless-smoke)
  set_tests_properties(tui_streaming_demo_smoke PROPERTIES TIMEOUT 90)
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
    add_dependencies(stun_xml_runtime_qualification test_tui_win32_conpty)
  endif()
endif()

if(TARGET gCanvas::Window OR TARGET flex_render_engine_gcanvas OR
   TARGET FlexUI::ControllerTurboScript)
  message(FATAL_ERROR "XML runtime gate must remain headless and independent of TurboScript JIT")
endif()
