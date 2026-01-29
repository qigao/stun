cmake_minimum_required(VERSION 3.16)
project(tui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library (zero dependencies)
add_library(tui INTERFACE)
target_include_directories(tui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Examples 
add_subdirectory(examples)
 