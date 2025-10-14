# NanoGUI Library CMakeLists.txt
cmake_minimum_required(VERSION 3.13)

# Extract project version from source
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/nanogui/common.h"
     nanogui_version_defines
     REGEX "#define NANOGUI_VERSION_(MAJOR|MINOR|PATCH) ")

foreach(ver ${nanogui_version_defines})
  if(ver MATCHES [[#define NANOGUI_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$]])
    set(NANOGUI_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endif()
endforeach()

if(NANOGUI_VERSION_PATCH MATCHES [[\.([a-zA-Z0-9]+)$]])
  set(NANOGUI_VERSION_TYPE "${CMAKE_MATCH_1}")
endif()

string(REGEX MATCH "^[0-9]+" NANOGUI_VERSION_PATCH "${NANOGUI_VERSION_PATCH}")

set(NANOGUI_VERSION
    "${NANOGUI_VERSION_MAJOR}.${NANOGUI_VERSION_MINOR}.${NANOGUI_VERSION_PATCH}"
)

message(STATUS "NanoGUI v${NANOGUI_VERSION} ${NANOGUI_VERSION_TYPE}")

# Glob up resource files
file(GLOB resources "${CMAKE_SOURCE_DIR}/resources/*.ttf"
     "${CMAKE_SOURCE_DIR}/${NANOGUI_GLOB}")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")

# Precompile .metal shaders to .metallib files
foreach(fname_in IN LISTS resources)
  if(NOT NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION
     AND NANOGUI_BACKEND STREQUAL "Metal"
     AND fname_in MATCHES "\\.metal")
    get_filename_component(fname_out ${fname_in} NAME)
    set(fname_out "${CMAKE_CURRENT_BINARY_DIR}/resources/${fname_out}lib")
    add_custom_command(
      OUTPUT ${fname_out}
      DEPENDS ${fname_in}
      COMMAND xcrun -sdk macosx metal -std=osx-metal2.0 -O3 "${fname_in}" -o
              "${fname_out}"
      VERBATIM)
  else()
    set(fname_out "${fname_in}")
  endif()
  list(APPEND resources_processed ${fname_out})
endforeach()

# Concatenate resource files into a comma separated string
string(REGEX REPLACE "([^\\]|^);" "\\1," resources_string
                     "${resources_processed}")
string(REGEX REPLACE "[\\](.)" "\\1" resources_string "${resources_string}")

# Create command line for running bin2c cmake script
set(bin2c_cmdline
    -DOUTPUT_C=nanogui_resources.cpp -DOUTPUT_H=nanogui_resources.h
    "-DINPUT_FILES=${resources_string}" -P
    "${CMAKE_SOURCE_DIR}/resources/bin2c.cmake")

# Run bin2c on resource files
add_custom_command(
  OUTPUT nanogui_resources.cpp nanogui_resources.h
  COMMAND ${CMAKE_COMMAND} ARGS ${bin2c_cmdline}
  DEPENDS ${resources} ${resources_processed}
  COMMENT "Running bin2c"
  VERBATIM)

# Determine backend-specific sources
set(NANOGUI_EXTRA "")

if(NANOGUI_BACKEND MATCHES "(OpenGL|GLES 2|GLES 3)")
  list(
    APPEND
    NANOGUI_EXTRA
    src/texture_gl.cpp
    src/shader_gl.cpp
    src/renderpass_gl.cpp
    src/opengl.cpp
    src/opengl_check.h)
endif()

if(NANOGUI_BACKEND STREQUAL "Metal")
  list(APPEND NANOGUI_EXTRA src/texture_metal.mm src/shader_metal.mm
       src/renderpass_metal.mm)
endif()

if(APPLE)
  list(APPEND NANOGUI_EXTRA src/darwin.mm src/autorelease.mm)
  set_property(SOURCE src/autorelease.mm PROPERTY COMPILE_FLAGS -fno-objc-arc)
endif()

# Core NanoGUI source files
set(NANOGUI_CORE_SOURCES
    # Backend-specific sources
    ${NANOGUI_EXTRA}
    # Fonts and resources
    nanogui_resources.cpp
    # Core sources
    include/nanogui/rtimer.h
    src/rtimer.cpp
    include/nanogui/common.h
    src/common.cpp
    include/nanogui/widget.h
    src/widget.cpp
    include/nanogui/theme.h
    src/theme.cpp
    include/nanogui/layout.h
    src/layout.cpp
    include/nanogui/label.h
    src/label.cpp
    include/nanogui/window.h
    src/window.cpp
    include/nanogui/popup.h
    src/popup.cpp
    include/nanogui/checkbox.h
    src/checkbox.cpp
    include/nanogui/button.h
    src/button.cpp
    include/nanogui/popupbutton.h
    src/popupbutton.cpp
    include/nanogui/combobox.h
    src/combobox.cpp
    include/nanogui/progressbar.h
    src/progressbar.cpp
    include/nanogui/slider.h
    src/slider.cpp
    include/nanogui/messagedialog.h
    src/messagedialog.cpp
    include/nanogui/textbox.h
    src/textbox.cpp
    include/nanogui/textarea.h
    src/textarea.cpp
    include/nanogui/imagepanel.h
    src/imagepanel.cpp
    include/nanogui/vscrollpanel.h
    src/vscrollpanel.cpp
    include/nanogui/colorwheel.h
    src/colorwheel.cpp
    include/nanogui/colorpicker.h
    src/colorpicker.cpp
    include/nanogui/graph.h
    src/graph.cpp
    include/nanogui/tabwidget.h
    src/tabwidget.cpp
    include/nanogui/canvas.h
    src/canvas.cpp
    include/nanogui/chroma.h
    src/chroma.cpp
    include/nanogui/texture.h
    src/texture.cpp
    include/nanogui/shader.h
    src/shader.cpp
    include/nanogui/quad.h
    src/quad.cpp
    include/nanogui/imageview.h
    src/imageview.cpp
    include/nanogui/traits.h
    src/traits.cpp
    include/nanogui/colorpass.h
    src/colorpass.cpp
    include/nanogui/renderpass.h
    include/nanogui/formhelper.h
    include/nanogui/icons.h
    include/nanogui/toolbutton.h
    include/nanogui/opengl.h
    include/nanogui.h)

# Add the appropriate backend source files
if(NANOGUI_BUILD_GLFW)
  list(APPEND NANOGUI_CORE_SOURCES include/nanogui/screen.h
       include/nanogui/screen_glfw.h src/screen.cpp src/screen_glfw.cpp)
endif()

if(NANOGUI_USE_SDL3)
  list(APPEND NANOGUI_CORE_SOURCES include/nanogui/screen.h
       include/nanogui/screen_sdl.h src/screen.cpp src/screen_sdl.cpp)
endif()

# Create the nanogui library target
add_library(nanogui ${NANOGUI_LIBRARY_TYPE} ${NANOGUI_CORE_SOURCES})

# Compiler-specific options
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(nanogui PRIVATE -fobjc-arc)
endif()

if(MSVC)
  # warning C4324: structure was padded due to alignment specifier
  target_compile_options(nanogui PRIVATE /wd4324)
endif()

# Compile definitions
target_compile_definitions(
  nanogui
  PUBLIC ${NANOGUI_BACKEND_DEFS}
  PRIVATE -DNANOGUI_BUILD)

if(NANOGUI_SKIP_STB_IMAGE_IMPLEMENTATION)
  target_compile_definitions(nanogui PRIVATE -DNVG_NO_STB_IMPLEMENTATION)
endif()

if(NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION)
  target_compile_definitions(nanogui
                             PUBLIC -DNANOGUI_SKIP_METAL_SHADER_PRECOMPILATION)
endif()

# Include directories
target_include_directories(
  nanogui
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Shared library definitions
if(NANOGUI_BUILD_SHARED)
  target_compile_definitions(
    nanogui
    PUBLIC -DNANOGUI_SHARED -DNVG_SHARED
    PRIVATE -DNVG_BUILD)

  if(NANOGUI_BUILD_GLAD)
    target_compile_definitions(
      nanogui
      PUBLIC -DGLAD_GLAPI_EXPORT
      PRIVATE -DGLAD_GLAPI_EXPORT_BUILD)
  endif()
endif()

# Link dependencies
if(NANOGUI_BUILD_GLAD)
  target_compile_definitions(nanogui PUBLIC -DNANOGUI_GLAD)
  target_link_libraries(nanogui PUBLIC glad::glad)
endif()

# Link GLFW backend
if(NANOGUI_BUILD_GLFW)
  target_link_libraries(nanogui PUBLIC ${_nanogui_glfw_target})
  target_compile_definitions(nanogui PUBLIC -DNANOGUI_BUILD_GLFW)
endif()

# Link SDL3 backend
if(NANOGUI_USE_SDL3)
  target_link_libraries(nanogui PUBLIC ${_nanogui_sdl3_target})
  target_compile_definitions(nanogui PUBLIC -DNANOGUI_USE_SDL3)
endif()

# Link nanovg library
target_link_libraries(nanogui PUBLIC nanovg::nanovg)

# Link nanovg_metal library if using Metal backend
if(NANOGUI_BACKEND STREQUAL "Metal")
  target_link_libraries(nanogui PRIVATE nanovg_metal::nanovg_metal)
endif()

# Link native file dialog library
target_link_libraries(nanogui PRIVATE $<BUILD_INTERFACE:nfd>)

# Link robin-map
target_link_libraries(nanogui PRIVATE tsl::robin_map)

# Link platform-specific libraries
if(NANOGUI_BUILD_SHARED)
  target_link_libraries(nanogui PRIVATE ${NANOGUI_LIBS})
  set_property(TARGET nanogui PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set_property(TARGET nanogui PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL
                                       ON)
  set_property(TARGET nanogui PROPERTY DEFINE_SYMBOL "")
  set_target_properties(nfd PROPERTIES POSITION_INDEPENDENT_CODE ON)

  # Set output directories for nanogui DLL
  set_target_properties(
    nanogui
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
               ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

  # For multi-config generators
  foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(
      nanogui
      PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}
                 "${CMAKE_BINARY_DIR}/bin"
                 LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}
                 "${CMAKE_BINARY_DIR}/bin"
                 ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}
                 "${CMAKE_BINARY_DIR}/bin")
  endforeach()

  # Automatically export all symbols from nanogui.dll (including nanovg)
  set_target_properties(nanogui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

  # Apply the export definitions to nanovg so its symbols are exported from
  # nanogui.dll
  target_compile_definitions(nanovg PRIVATE NVG_BUILD)
else()
  target_link_libraries(nanogui PUBLIC ${NANOGUI_LIBS})
endif()

# Auto-detect GLES include directory on Raspberry PI
if(EXISTS /opt/vc/include)
  target_include_directories(nanogui PUBLIC /opt/vc/include)
endif()

set_target_properties(nanogui PROPERTIES FOLDER nanogui)
if(NANOGUI_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(
    TARGETS nanogui
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  install(
    DIRECTORY include/nanogui
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h")

  install(
    DIRECTORY vendor/nanovg/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanovg
    FILES_MATCHING
    PATTERN "*.h")

  install(TARGETS nanogui nanovg EXPORT nanoguiTargets)

  set(NANOGUI_CMAKECONFIG_INSTALL_DIR
      "${CMAKE_INSTALL_DATAROOTDIR}/cmake/nanogui")

  configure_package_config_file(
    resources/nanoguiConfig.cmake.in nanoguiConfig.cmake
    INSTALL_DESTINATION ${NANOGUI_CMAKECONFIG_INSTALL_DIR})

  write_basic_package_version_file(
    nanoguiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion ARCH_INDEPENDENT)

  install(EXPORT nanoguiTargets DESTINATION ${NANOGUI_CMAKECONFIG_INSTALL_DIR})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nanoguiConfigVersion.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/nanoguiConfig.cmake
          DESTINATION ${NANOGUI_CMAKECONFIG_INSTALL_DIR})
endif()
