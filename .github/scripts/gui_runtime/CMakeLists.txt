cmake_minimum_required(VERSION 3.20...3.31)
project(StunVisibleGuiQualification LANGUAGES C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

# Build the ACTUAL production C++ GUI, not a fake widget or software-only
# alternative renderer. Use the same installed Salts and TurboScript SDKs
# required by production, with a single real OpenGL/GLFW presentation path.
find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_package(SaltsUtils CONFIG REQUIRED PATHS "$ENV{SALTS_UTILS_ROOT}" NO_DEFAULT_PATH)
find_package(Threads REQUIRED)
find_program(RE2C_EXECUTABLE re2c REQUIRED)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
set(NANOGUI_BUILD_EXAMPLES ON)
set(NANOGUI_BUILD_GLFW ON)
set(NANOGUI_USE_SDL3 OFF)
set(NANOGUI_BUILD_SHARED OFF)
set(FLEXUI_ENABLE_PLUGINS ON)
set(FLEXUI_ENABLE_TURBOSCRIPT ON)
set(TURBOSCRIPT_ROOT "$ENV{TURBOSCRIPT_ROOT}")
set(GCANVAS_BUILD_OPENGL ON)
set(GCANVAS_BUILD_GLES OFF)
set(GCANVAS_BUILD_ANDROID_EGL OFF)
set(GCANVAS_BUILD_VULKAN OFF)
set(GCANVAS_BUILD_SVG ON)
set(GCANVAS_BUILD_WINDOW ON)
set(GCANVAS_BUILD_GPU_TESTS OFF)
set(GCANVAS_BUILD_TESTS OFF)
set(GCANVAS_BUILD_EXAMPLES OFF)
set(FLEX_BUILD_RENDER_ENGINE_GCANVAS ON)
set(FLEX_BUILD_BACKEND_TUI OFF)
set(FLEX_BUILD_BACKEND_OPENGL ON)
set(FLEX_BUILD_BACKEND_VULKAN OFF)
set(FLEX_BUILD_BACKEND_D2D OFF)
set(FLEX_BUILD_BINARY OFF)
set(FLEX_BUILD_TOOLS OFF)

# Original production targets/source lists from the same Stun source HEAD.
include_directories(
    "${STUN_ROOT}/vendor/tinyfsm"
    "${STUN_ROOT}/vendor/mir"
    "${STUN_ROOT}/vendor/miniblas"
    "${STUN_ROOT}/tools/tree-sitter/plugins")
add_executable(stun_gui_lemon "${STUN_ROOT}/tools/lemon/lemon.c")
set(LEMON_EXECUTABLE "$<TARGET_FILE:stun_gui_lemon>")
set(LEMON_DEPENDS stun_gui_lemon)
set(LEMPAR "${STUN_ROOT}/tools/lemon/lempar.c")
add_subdirectory("${STUN_ROOT}/vendor/mir" mir)
add_subdirectory("${STUN_ROOT}/vendor/miniblas" miniblas)
add_subdirectory("${STUN_ROOT}/shared/gcanvas" gcanvas)
add_subdirectory("${STUN_ROOT}/shared/markdown" markdown)
add_subdirectory("${STUN_ROOT}/tango" tango)
add_subdirectory("${STUN_ROOT}/flex" flex)
add_subdirectory("${STUN_ROOT}/flexUI" flexUI)
target_link_libraries(flexUI PRIVATE Stun::UICore)

# The official TurboScript.Native SDK depends on the ONE canonical
# SaltsUtils DataBind DSO. Linux linkers reject its unresolved data_bind_*
# and cserde_* symbols unless that published provider is on the consumer
# link line. Never fake, vendor or ignore missing SDK symbols.
if(NOT TARGET Salts::DataBind)
  message(FATAL_ERROR "TurboScript Host ABI requires the published Salts::DataBind target from SaltsUtils")
endif()
get_target_property(_stun_gui_databind_imported Salts::DataBind IMPORTED)
if(NOT _stun_gui_databind_imported)
  message(FATAL_ERROR "GUI requires an imported Salts::DataBind from the installed SDK")
endif()
target_link_libraries(flexui_desktop_editor PRIVATE Salts::DataBind)

if(NOT TARGET gCanvas::Window OR NOT TARGET gCanvas::OpenGL OR
   NOT TARGET FlexUI::GCanvasPluginWindowHost OR
   NOT TARGET FlexUI::ControllerTurboScript OR
   NOT TARGET flexui_desktop_editor)
  message(FATAL_ERROR "real visible Stun desktop editor GPU/PluginHost/TurboScript targets were not created")
endif()
