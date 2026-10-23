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
# Optional examples/window/GPU consumers remain disabled. The installed JIT
# contracts are an explicit opt-in, separate from ordinary XML qualification.
set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
set(NANOGUI_BUILD_EXAMPLES OFF)
set(FLEXUI_ENABLE_PLUGINS OFF)
option(STUN_XML_RUNTIME_TURBOSCRIPT
  "Execute installed TurboScript interpreter/JIT contracts without a window" OFF)
set(FLEXUI_ENABLE_TURBOSCRIPT "${STUN_XML_RUNTIME_TURBOSCRIPT}")
foreach(option IN ITEMS GCANVAS_BUILD_OPENGL GCANVAS_BUILD_GLES
    GCANVAS_BUILD_ANDROID_EGL GCANVAS_BUILD_VULKAN GCANVAS_BUILD_SVG
    GCANVAS_BUILD_WINDOW GCANVAS_BUILD_TESTS GCANVAS_BUILD_EXAMPLES
    FLEX_BUILD_RENDER_ENGINE_GCANVAS FLEX_BUILD_RENDER_ENGINE_NANOVG
    FLEX_BUILD_BACKEND_OPENGL FLEX_BUILD_BACKEND_VULKAN FLEX_BUILD_BACKEND_D2D
    FLEX_BUILD_BACKEND_TUI FLEX_BUILD_BINARY FLEX_BUILD_TOOLS)
  set(${option} OFF CACHE BOOL "Not part of headless XML qualification" FORCE)
endforeach()
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
if(STUN_XML_RUNTIME_TURBOSCRIPT)
  add_custom_target(stun_turboscript_qualification)
  foreach(test IN ITEMS test_turboscript_controller test_turboscript_application
      test_desktop_editor_controller)
    add_executable(${test} "${STUN_ROOT}/flexUI/tests/${test}.cpp")
    target_link_libraries(${test} PRIVATE FlexUI::ControllerTurboScript Salts::TinyTest)
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(${test} PROPERTIES TIMEOUT 120)
    add_dependencies(stun_turboscript_qualification ${test})
  endforeach()
  target_compile_definitions(test_desktop_editor_controller PRIVATE
    FLEXUI_EDITOR_CONTROLLER_PATH="${STUN_ROOT}/flexUI/examples/desktop_editor/editor.tbs")
elseif(TARGET FlexUI::ControllerTurboScript)
  message(FATAL_ERROR "The ordinary XML gate must remain independent of TurboScript JIT")
endif()
if(TARGET gCanvas::Window OR TARGET flex_render_engine_gcanvas)
  message(FATAL_ERROR "XML and optional TurboScript contracts must remain headless")
endif()
