cmake_minimum_required (VERSION 3.13..3.18)

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0) # macOS Big Sur (released Nov 2020, EOL Sep 2023)

# Extract project version from source
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/nanogui/common.h"
  nanogui_version_defines REGEX "#define NANOGUI_VERSION_(MAJOR|MINOR|PATCH) ")

foreach(ver ${nanogui_version_defines})
  if(ver MATCHES [[#define NANOGUI_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$]])
      set(NANOGUI_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endif()
endforeach()

if(NANOGUI_VERSION_PATCH MATCHES [[\.([a-zA-Z0-9]+)$]])
  set(NANOGUI_VERSION_TYPE "${CMAKE_MATCH_1}")
endif()

string(REGEX MATCH "^[0-9]+" NANOGUI_VERSION_PATCH "${NANOGUI_VERSION_PATCH}")

set(NANOGUI_VERSION "${NANOGUI_VERSION_MAJOR}.${NANOGUI_VERSION_MINOR}.${NANOGUI_VERSION_PATCH}")

message(STATUS "NanoGUI v${NANOGUI_VERSION} ${NANOGUI_VERSION_TYPE}")

project(nanogui
  DESCRIPTION
    "NanoGUI"
  LANGUAGES
    CXX C
  VERSION
    ${NANOGUI_VERSION}
)

# Dependencies are now managed by vcpkg (see vcpkg.json)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

set(NANOGUI_MASTER_PROJECT OFF)
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(NANOGUI_MASTER_PROJECT ON)
endif()

set(NANOGUI_BUILD_GLFW_DEFAULT OFF)
set(NANOGUI_USE_SDL3_DEFAULT ON)

if (WIN32)
  set(NANOGUI_BUILD_GLAD_DEFAULT ON)
else()
  set(NANOGUI_BUILD_GLAD_DEFAULT OFF)
endif()

set(NANOGUI_BUILD_SHARED_DEFAULT ON) 
set(NANOGUI_USE_SDL2_DEFAULT OFF)

if (CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
  set(CMAKE_CXX_COMPILER_ID "Emscripten")
  set(NANOGUI_BUILD_SHARED_DEFAULT OFF)
  set(NANOGUI_BUILD_GLAD_DEFAULT   OFF) 
  set(NANOGUI_BUILD_GLFW_DEFAULT  OFF)
  set(NANOGUI_USE_SDL3_DEFAULT     ON)

  set(CMAKE_STATIC_LIBRARY_SUFFIX ".bc")
  set(CMAKE_EXECUTABLE_SUFFIX ".bc")
  set(CMAKE_CXX_CREATE_STATIC_LIBRARY "<CMAKE_CXX_COMPILER> -o <TARGET> <LINK_FLAGS> <OBJECTS>")
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    add_compile_options(-O3 -DNDEBUG)
  endif()
endif()

option(NANOGUI_BUILD_EXAMPLES            "Build NanoGUI example application?" ON)
option(NANOGUI_BUILD_SHARED              "Build NanoGUI as a shared library?" ${NANOGUI_BUILD_SHARED_DEFAULT})
option(NANOGUI_BUILD_GLAD                "Build GLAD OpenGL loader library? (needed on Windows)" ${NANOGUI_BUILD_GLAD_DEFAULT})
option(NANOGUI_INSTALL                   "Install NanoGUI on `make install`?" ON)
option(NANOGUI_WITH_RLOTTIE              "Enable rlottie integration for demos?" OFF)

# Backend selection (mutually exclusive)
include(CMakeDependentOption)
option(NANOGUI_BUILD_GLFW "Use GLFW backend (desktop support)" ${NANOGUI_BUILD_GLFW_DEFAULT})
option(NANOGUI_USE_SDL3 "Use SDL3 backend (cross-platform support)" ${NANOGUI_USE_SDL3_DEFAULT})

# Validate backend selection - exactly one backend must be enabled
if (NANOGUI_BUILD_GLFW AND NANOGUI_USE_SDL3)
  message(FATAL_ERROR "NanoGUI: Cannot enable both GLFW and SDL3 backends. Choose one:\n"
                      "  -DNANOGUI_BUILD_GLFW=ON   (for GLFW backend)\n"
                      "  -DNANOGUI_USE_SDL3=ON     (for SDL3 backend)")
endif()

if (NOT NANOGUI_BUILD_GLFW AND NOT NANOGUI_USE_SDL3)
  message(FATAL_ERROR "NanoGUI: At least one backend must be enabled:\n"
                      "  -DNANOGUI_BUILD_GLFW=ON   (for GLFW backend)\n"
                      "  -DNANOGUI_USE_SDL3=ON     (for SDL3 backend)")
endif()

set(NANOGUI_RLOTTIE_FOUND OFF)
if (NANOGUI_WITH_RLOTTIE)
  find_path(RLOTTIE_INCLUDE_DIR rlottie.h)
  find_library(RLOTTIE_LIBRARY NAMES rlottie)
  if (RLOTTIE_INCLUDE_DIR AND RLOTTIE_LIBRARY)
    set(NANOGUI_RLOTTIE_FOUND ON)
  else()
    message(WARNING "rlottie not found; disabling rlottie integration")
    set(NANOGUI_WITH_RLOTTIE OFF)
  endif()
endif()

option(NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION "Compile Metal shaders at runtime instead of precompiling them while nanogui is built." OFF)
option(NANOGUI_SKIP_STB_IMAGE_IMPLEMENTATION    "Do not compile stb_image's implementation into nanogui. If enabled, user code must do this instead." OFF)

if (NOT NANOGUI_BACKEND)
  if (CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
    set(NANOGUI_BACKEND_DEFAULT "GLES 2")
  elseif (APPLE)
    set(NANOGUI_BACKEND_DEFAULT "Metal")
  elseif (CMAKE_SYSTEM_NAME MATCHES "Linux" OR
          CMAKE_SYSTEM_NAME MATCHES "BSD")
    set(NANOGUI_BACKEND_DEFAULT "OpenGL")
  else()
    set(NANOGUI_BACKEND_DEFAULT "OpenGL")
  endif()

  set(NANOGUI_BACKEND ${NANOGUI_BACKEND_DEFAULT} CACHE STRING "Choose the backend used for rendering (OpenGL/GLES 2/GLES 3/Metal)" FORCE)
endif()

set_property(CACHE NANOGUI_BACKEND PROPERTY STRINGS "OpenGL" "GLES 2" "GLES 3" "Metal")

# Required libraries, flags, and include files for compiling and linking against nanogui (all targets)
set(NANOGUI_EXTRA_INCS "")

include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)

macro(CHECK_CXX_COMPILER_AND_LINKER_FLAGS _RESULT _CXX_FLAGS _LINKER_FLAGS)
  set(CMAKE_REQUIRED_FLAGS ${_CXX_FLAGS})
  set(CMAKE_REQUIRED_LIBRARIES ${_LINKER_FLAGS})
  set(CMAKE_REQUIRED_QUIET TRUE)
  check_cxx_source_runs("#include <iostream>\nint main(int argc, char **argv) { std::cout << \"test\"; return 0; }" ${_RESULT})
  set(CMAKE_REQUIRED_FLAGS "")
  set(CMAKE_REQUIRED_LIBRARIES "")
endmacro()

if (NANOGUI_BACKEND MATCHES "(OpenGL|GLES 2|GLES 3)")
  list(APPEND NANOGUI_EXTRA
    src/texture_gl.cpp src/shader_gl.cpp
    src/renderpass_gl.cpp src/opengl.cpp
    src/opengl_check.h
  )
endif()

if (NANOGUI_BACKEND STREQUAL "OpenGL")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_OPENGL)
  set(NANOGUI_GLOB "resources/*.gl")
  message(STATUS "NanoGUI: using OpenGL backend.")
elseif (NANOGUI_BACKEND STREQUAL "GLES 2")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_GLES -DNANOGUI_GLES_VERSION=2)
  set(NANOGUI_GLOB "resources/*.gles")
  message(STATUS "NanoGUI: using GLES 2 backend.")
elseif (NANOGUI_BACKEND STREQUAL "GLES 3")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_GLES -DNANOGUI_GLES_VERSION=3)
  set(NANOGUI_GLOB "resources/*.gles")
  message(STATUS "NanoGUI: using GLES 3 backend.")
elseif (NANOGUI_BACKEND STREQUAL "Metal")
  set(NANOGUI_BACKEND_DEFS -DNANOGUI_USE_METAL)
  list(APPEND NANOGUI_EXTRA
    src/texture_metal.mm src/shader_metal.mm src/renderpass_metal.mm
  )
  set(NANOGUI_GLOB "resources/*.metal")
  message(STATUS "NanoGUI: using Metal backend.")
endif()

# Shared library mode: add dllimport/dllexport flags to all symbols
if (NANOGUI_BUILD_SHARED)
  message(STATUS "NanoGUI: building shared library.")
else()
  message(STATUS "NanoGUI: building static library.")
endif()

if (MSVC)
  # Disable annoying MSVC warnings (all targets)
  add_definitions(/D "_CRT_SECURE_NO_WARNINGS")

  # Parallel build on MSVC (all targets)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Compile with compiler warnings turned on
if (MSVC)
  if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Use C++17, visibility=hidden by default, interprocedural optimization
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Prefer libc++ in conjunction with Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
  CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP "-stdlib=libc++" "-stdlib=libc++")
  if (APPLE OR HAS_LIBCPP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
    message(STATUS "NanoGUI: using libc++.")
  else()
    CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP_AND_CPPABI "-stdlib=libc++" "-stdlib=libc++ -lc++abi")
    if (HAS_LIBCPP_AND_CPPABI)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
      message(STATUS "NanoGUI: using libc++ and libc++abi.")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libstdc++")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libstdc++")
      message(STATUS "NanoGUI: using libstdc++.")
    endif()
  endif()
endif()

if (NANOGUI_BUILD_GLAD)
  # Use GLAD from vcpkg
  find_package(glad CONFIG REQUIRED)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "BSD")
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
  if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    include_directories(/usr/X11R6/include/)
    link_directories(/usr/X11R6/lib)
  endif()
endif()

# Adapt to variations in rpath handling on Linux and macOS
if (APPLE)
  set(NANOGUI_ORIGIN "@loader_path")
elseif(UNIX)
  set(NANOGUI_ORIGIN "$ORIGIN")
endif()

# Required core libraries on various platforms
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Emscripten")
  # No dependencies
elseif (WIN32)
  list(APPEND NANOGUI_LIBS opengl32)
elseif (APPLE)
  find_library(cocoa_library Cocoa)
  find_library(iokit_library IOKit)
  list(APPEND NANOGUI_LIBS ${cocoa_library} ${iokit_library})
  mark_as_advanced(cocoa_library iokit_library)

  if (NANOGUI_BACKEND STREQUAL "OpenGL")
    find_library(corevideo_library CoreVideo)
    find_library(opengl_library OpenGL)

    # Disable OpenGL deprecation warnings
    add_definitions(-DGL_SILENCE_DEPRECATION)
    list(APPEND NANOGUI_LIBS ${opengl_library} ${corevideo_library})
    mark_as_advanced(opengl_library corevideo_library)
  else()
    find_library(metal_library Metal)
    list(APPEND NANOGUI_LIBS ${metal_library})
    mark_as_advanced(metal_library)
  endif()
  find_library(quartzcore_library QuartzCore)
  mark_as_advanced(quartzcore_library)
  list(APPEND NANOGUI_EXTRA src/darwin.mm src/autorelease.mm)
  list(APPEND NANOGUI_LIBS ${quartzcore_library})
  set_property(SOURCE src/autorelease.mm PROPERTY COMPILE_FLAGS -fno-objc-arc)
elseif (CMAKE_SYSTEM MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "BSD")
  list(APPEND NANOGUI_LIBS X11 pthread)
  if (NANOGUI_BACKEND STREQUAL "OpenGL")
    list(APPEND NANOGUI_LIBS GL)
  elseif (NANOGUI_BACKEND STREQUAL "GLES 2")
    list(APPEND NANOGUI_LIBS GLESv2)
  elseif (NANOGUI_BACKEND STREQUAL "GLES 3")
    list(APPEND NANOGUI_LIBS GLESv2) # Most systems expose ESv3 functionality through GLESv2
  endif()
  if (NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    list(APPEND NANOGUI_LIBS rt)
  endif()
  if (CMAKE_SYSTEM MATCHES "Linux")
    list(APPEND NANOGUI_LIBS dl)
  endif()
endif()

# Glob up resource files
file(GLOB resources
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.ttf"
  "${NANOGUI_GLOB}")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")

# Precompile .metal shaders to .metallib files
foreach(fname_in IN LISTS resources)
  if (NOT NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION AND NANOGUI_BACKEND STREQUAL "Metal" AND fname_in MATCHES "\\.metal")
    get_filename_component(fname_out ${fname_in} NAME)
    set(fname_out "${CMAKE_CURRENT_BINARY_DIR}/resources/${fname_out}lib")
    add_custom_command(
        OUTPUT ${fname_out}
        DEPENDS ${fname_in}
        COMMAND xcrun -sdk macosx metal -std=osx-metal2.0 -O3 "${fname_in}" -o "${fname_out}"
        VERBATIM
    )
  else()
    set(fname_out "${fname_in}")
  endif()
  list(APPEND resources_processed ${fname_out})
endforeach()

# Concatenate resource files into a comma separated string
string(REGEX REPLACE "([^\\]|^);" "\\1," resources_string "${resources_processed}")
string(REGEX REPLACE "[\\](.)" "\\1" resources_string "${resources_string}")

# Create command line for running bin2c cmake script
set(bin2c_cmdline
  -DOUTPUT_C=nanogui_resources.cpp
  -DOUTPUT_H=nanogui_resources.h
  "-DINPUT_FILES=${resources_string}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/resources/bin2c.cmake")


# Run bin2c on resource files
add_custom_command(
  OUTPUT nanogui_resources.cpp nanogui_resources.h
  COMMAND ${CMAKE_COMMAND} ARGS ${bin2c_cmdline}
  DEPENDS ${resources} ${resources_processed}
  COMMENT "Running bin2c"
  VERBATIM)

# Needed to generated files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Set library type
if (NANOGUI_BUILD_SHARED)
  set(NANOGUI_LIBRARY_TYPE "SHARED")
else()
  set(NANOGUI_LIBRARY_TYPE "STATIC")
endif()

# Add vendor libraries before creating nanogui target
add_subdirectory(vendor)

# Compile main NanoGUI library
set(NANOGUI_CORE_SOURCES
  # Backend-specific sources
  ${NANOGUI_EXTRA}

  # Fonts etc.
  nanogui_resources.cpp
  src/rtimer.cpp src/rtimer.h
  include/nanogui/common.h src/common.cpp
  include/nanogui/widget.h src/widget.cpp
  include/nanogui/theme.h src/theme.cpp
 
  include/nanogui/layout.h src/layout.cpp
  include/nanogui/label.h src/label.cpp
  include/nanogui/window.h src/window.cpp
  include/nanogui/popup.h src/popup.cpp
  include/nanogui/checkbox.h src/checkbox.cpp
  include/nanogui/button.h src/button.cpp
  include/nanogui/popupbutton.h src/popupbutton.cpp
  include/nanogui/combobox.h src/combobox.cpp
  include/nanogui/progressbar.h src/progressbar.cpp
  include/nanogui/slider.h src/slider.cpp
  include/nanogui/messagedialog.h src/messagedialog.cpp
  include/nanogui/textbox.h src/textbox.cpp
  include/nanogui/textarea.h src/textarea.cpp
  include/nanogui/imagepanel.h src/imagepanel.cpp
  include/nanogui/vscrollpanel.h src/vscrollpanel.cpp
  include/nanogui/colorwheel.h src/colorwheel.cpp
  include/nanogui/colorpicker.h src/colorpicker.cpp
  include/nanogui/graph.h src/graph.cpp
  include/nanogui/tabwidget.h src/tabwidget.cpp
  include/nanogui/canvas.h src/canvas.cpp
  include/nanogui/chroma.h src/chroma.cpp
  include/nanogui/texture.h src/texture.cpp
  include/nanogui/shader.h src/shader.cpp
  include/nanogui/quad.h src/quad.cpp
  include/nanogui/imageview.h src/imageview.cpp
  include/nanogui/traits.h src/traits.cpp
  include/nanogui/colorpass.h src/colorpass.cpp
  include/nanogui/renderpass.h
  include/nanogui/formhelper.h
  include/nanogui/icons.h
  include/nanogui/toolbutton.h
  include/nanogui/opengl.h
  include/nanogui/nanogui.h
)

# Add the appropriate backend source files
# Both backends use include/nanogui/screen.h but different implementations
if (NANOGUI_BUILD_GLFW)
  list(APPEND NANOGUI_CORE_SOURCES
    include/nanogui/screen.h
    include/nanogui/screen_glfw.h
    src/screen.cpp
    src/screen_glfw.cpp
  )
  message(STATUS "NanoGUI: Using GLFW backend (Screen class)")
endif()

if (NANOGUI_USE_SDL3)
  list(APPEND NANOGUI_CORE_SOURCES
    include/nanogui/screen.h
    include/nanogui/screen_sdl.h
    src/screen.cpp
    src/screen_sdl.cpp
  )
  message(STATUS "NanoGUI: Using SDL3 backend (Screen class)")
endif()

add_library(nanogui ${NANOGUI_LIBRARY_TYPE} ${NANOGUI_CORE_SOURCES})

if (APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Use automatic reference counting for Objective-C portions
  target_compile_options(nanogui PRIVATE -fobjc-arc)
endif()

target_compile_definitions(nanogui
  PUBLIC
    ${NANOGUI_BACKEND_DEFS}

  PRIVATE
    -DNANOGUI_BUILD
)

if (NANOGUI_SKIP_STB_IMAGE_IMPLEMENTATION)
  # Frameworks that statically link to nanobind and which already include the
  # stb_image implementation bits can set this flag to avoid duplicate symbol
  # errors.

  target_compile_definitions(nanogui
    PRIVATE -DNVG_NO_STB_IMPLEMENTATION)
endif()
# Link GLFW backend (legacy desktop support)
if (NANOGUI_BUILD_GLFW)
  set(_nanogui_glfw_target "")
  find_package(glfw3 CONFIG QUIET)
  if (glfw3_FOUND)
    if (TARGET glfw)
      set(_nanogui_glfw_target glfw)
    elseif (TARGET glfw3)
      set(_nanogui_glfw_target glfw3)
    elseif (TARGET glfw::glfw)
      set(_nanogui_glfw_target glfw::glfw)
    endif()
  endif()
  if (_nanogui_glfw_target STREQUAL "")
    find_package(GLFW3 REQUIRED)
    if (TARGET GLFW::GLFW)
      set(_nanogui_glfw_target GLFW::GLFW)
    elseif (DEFINED GLFW3_LIBRARY)
      set(_nanogui_glfw_target ${GLFW3_LIBRARY})
    elseif (DEFINED GLFW3_LIBRARIES)
      set(_nanogui_glfw_target ${GLFW3_LIBRARIES})
    endif()
    if (DEFINED GLFW3_INCLUDE_DIRS)
      target_include_directories(nanogui PUBLIC ${GLFW3_INCLUDE_DIRS})
    endif()
  endif()
  if (_nanogui_glfw_target STREQUAL "")
    message(FATAL_ERROR "NanoGUI: unable to locate a GLFW target to link against.")
  endif()
  target_link_libraries(nanogui PUBLIC ${_nanogui_glfw_target})
  target_compile_definitions(nanogui PUBLIC -DNANOGUI_BUILD_GLFW)
  message(STATUS "NanoGUI: GLFW backend linked - Screen available for desktop support.")
endif()

# Link SDL3 backend (cross-platform support)
if (NANOGUI_USE_SDL3)
  set(_nanogui_sdl3_target "")
  find_package(SDL3 CONFIG QUIET)
  if (SDL3_FOUND)
    if (TARGET SDL3::SDL3)
      set(_nanogui_sdl3_target SDL3::SDL3)
    elseif (TARGET SDL3::SDL3-shared)
      set(_nanogui_sdl3_target SDL3::SDL3-shared)
    elseif (TARGET SDL3::SDL3-static)
      set(_nanogui_sdl3_target SDL3::SDL3-static)
    endif()
  endif()
  if (_nanogui_sdl3_target STREQUAL "")
    find_package(SDL3 REQUIRED)
    if (TARGET SDL3::SDL3)
      set(_nanogui_sdl3_target SDL3::SDL3)
    elseif (DEFINED SDL3_LIBRARY)
      set(_nanogui_sdl3_target ${SDL3_LIBRARY})
    elseif (DEFINED SDL3_LIBRARIES)
      set(_nanogui_sdl3_target ${SDL3_LIBRARIES})
    endif()
    if (DEFINED SDL3_INCLUDE_DIRS)
      target_include_directories(nanogui PUBLIC ${SDL3_INCLUDE_DIRS})
    endif()
  endif()
  if (_nanogui_sdl3_target STREQUAL "")
    message(FATAL_ERROR "NanoGUI: unable to locate an SDL3 target to link against.")
  endif()
  target_link_libraries(nanogui PUBLIC ${_nanogui_sdl3_target})
  target_compile_definitions(nanogui PUBLIC -DNANOGUI_USE_SDL3)
  message(STATUS "NanoGUI: SDL3 backend linked - Screen available for cross-platform support.")
endif()

if (NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION)
  message(STATUS "NanoGUI: skipping precompilation of Metal shaders.")
  target_compile_definitions(nanogui
    PUBLIC -DNANOGUI_SKIP_METAL_SHADER_PRECOMPILATION)
endif()

# Use robin-map from vcpkg
find_package(tsl-robin-map CONFIG REQUIRED)
target_link_libraries(nanogui PRIVATE tsl::robin_map)

target_include_directories(nanogui
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>

  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

if (NANOGUI_BUILD_SHARED)
  target_compile_definitions(nanogui
    PUBLIC
      -DNANOGUI_SHARED -DNVG_SHARED
    PRIVATE
      -DNVG_BUILD
  )

  if (NANOGUI_BUILD_GLAD)
    target_compile_definitions(nanogui
      PUBLIC
        -DGLAD_GLAPI_EXPORT
      PRIVATE
        -DGLAD_GLAPI_EXPORT_BUILD
    )
  endif()
endif()

if (NANOGUI_BUILD_GLAD)
  target_compile_definitions(nanogui PUBLIC -DNANOGUI_GLAD)
  target_link_libraries(nanogui PUBLIC glad::glad)
endif()

# Link nanovg library
target_link_libraries(nanogui PUBLIC nanovg::nanovg)

# Link nanovg_metal library if using Metal backend
if (NANOGUI_BACKEND STREQUAL "Metal")
  target_link_libraries(nanogui PRIVATE nanovg_metal::nanovg_metal)
endif()

# Link native file dialog library
target_link_libraries(nanogui PRIVATE $<BUILD_INTERFACE:nfd>)

if (NANOGUI_BUILD_SHARED)
  target_link_libraries(nanogui PRIVATE ${NANOGUI_LIBS})
  set_property(TARGET nanogui PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set_property(TARGET nanogui PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  set_property(TARGET nanogui PROPERTY DEFINE_SYMBOL "")
  set_target_properties(nfd PROPERTIES POSITION_INDEPENDENT_CODE ON)
  
  # Set output directories for nanogui DLL
  set_target_properties(nanogui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
  
  # For multi-config generators
  foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(nanogui PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
      LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
      ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin"
    )
  endforeach()
else()
  target_link_libraries(nanogui PUBLIC ${NANOGUI_LIBS})
endif()

if (MSVC)
  # warning C4324: "...": structure was padded due to alignment specifier
  target_compile_options(nanogui PRIVATE /wd4324)
endif()

# Auto-detect GLES include directory on Raspberry PI
if (EXISTS /opt/vc/include)
  target_include_directories(nanogui PUBLIC /opt/vc/include)
endif()

if (NANOGUI_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(TARGETS nanogui
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  install(DIRECTORY include/nanogui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h")

  install(DIRECTORY vendor/nanovg/src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanovg
          FILES_MATCHING PATTERN "*.h")

  install(TARGETS nanogui nanovg EXPORT nanoguiTargets)

  set(NANOGUI_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_DATAROOTDIR}/cmake/nanogui")

  configure_package_config_file(
    resources/nanoguiConfig.cmake.in nanoguiConfig.cmake
    INSTALL_DESTINATION ${NANOGUI_CMAKECONFIG_INSTALL_DIR})

  write_basic_package_version_file(
    nanoguiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion ARCH_INDEPENDENT)

  install(
    EXPORT nanoguiTargets
    DESTINATION ${NANOGUI_CMAKECONFIG_INSTALL_DIR})

  install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/nanoguiConfigVersion.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/nanoguiConfig.cmake
    DESTINATION ${NANOGUI_CMAKECONFIG_INSTALL_DIR})
endif()

# Output files directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# For multi-config generators (Visual Studio, Ninja Multi-Config), prevent config subdirectories
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin")
endforeach()

add_subdirectory(theme)
add_subdirectory(examples)
add_subdirectory(ffmpeg)
# vim: set et ts=2 sw=2 ft=cmake nospell:
