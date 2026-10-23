cmake_minimum_required(VERSION 3.16)

# Permit the C core gate without configuring the full Stun GUI.  The installed
# Salts SDK remains mandatory so standalone and integrated builds have one
# Unicode implementation and identical input semantics.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunTango LANGUAGES C)
    include(CTest)
    find_package(Salts CONFIG REQUIRED)
endif()
set(TARGET_NAME tango)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON) 

# TUI library sources
set(TUI_SOURCES
    src/tui_buffer.c
    src/tui_buffer_ext.c
    src/tui_terminal.c
    src/tui_input.c
    src/tui_app.c
    src/tui_async.c
    src/tui_widgets.c
    src/tui_layout.c
    src/tui_advanced.c 
)

add_library(${TARGET_NAME} STATIC ${TUI_SOURCES})
if(WIN32)
    # Keep windows.h from importing legacy Winsock before Salts::Core's
    # Winsock2-backed public headers reach a Tango translation unit.
    target_compile_definitions(${TARGET_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()
if(NOT TARGET Salts::Unicode)
    message(FATAL_ERROR "Tango requires the canonical installed Salts::Unicode target")
endif()
if(NOT TARGET Salts::Platform)
    message(FATAL_ERROR "Tango async mailbox requires the installed Salts::Platform target")
endif()
target_link_libraries(${TARGET_NAME} PUBLIC Salts::Unicode Salts::Platform)

# tui_advanced.c uses sqrtf; static consumers need the math dependency.
if(UNIX)
    target_link_libraries(${TARGET_NAME} PUBLIC m)
endif()

target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
 
 
# Examples
if(NANOGUI_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

set_target_properties(${TARGET_NAME}  PROPERTIES FOLDER "tango")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
