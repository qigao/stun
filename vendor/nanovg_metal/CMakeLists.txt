cmake_minimum_required(VERSION 3.13)

project(nanovg_metal LANGUAGES C OBJC)

# Add the nanovg_metal static library
add_library(nanovg_metal STATIC
    src/nanovg_mtl.m
    src/nanovg_mtl.h
)

# Create an alias for consistent naming
add_library(nanovg_metal::nanovg_metal ALIAS nanovg_metal)

# Set language standards
set_target_properties(nanovg_metal PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Include directories
target_include_directories(nanovg_metal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/nanovg_metal>
)

# Platform check - Metal is only available on Apple platforms
if(NOT APPLE)
    message(FATAL_ERROR "nanovg_metal requires macOS or iOS")
endif()

# Find required frameworks
find_library(METAL_LIBRARY Metal REQUIRED)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)

# Link frameworks
target_link_libraries(nanovg_metal PUBLIC
    ${METAL_LIBRARY}
    ${QUARTZCORE_LIBRARY}
)

# Compiler options for Objective-C
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(nanovg_metal PRIVATE -fobjc-arc)
endif()

# Suppress warnings
if(MSVC)
    target_compile_options(nanovg_metal PRIVATE /wd4005 /wd4456 /wd4457)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nanovg_metal PRIVATE -Wno-unused-result)
endif()
