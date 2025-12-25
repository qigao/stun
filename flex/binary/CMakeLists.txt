# Flex Binary Format Library
cmake_minimum_required(VERSION 3.16)

# Find zstd for compression
find_package(zstd CONFIG REQUIRED)

# ============================================================================
# flex_binary - Binary serialization library
# ============================================================================

add_library(flex_binary STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/binary/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/binary/writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/binary/reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/binary/compiler.cpp
)

target_include_directories(flex_binary
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(flex_binary
    PUBLIC flex flex_compiler flex_runtime
    PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
)

set_target_properties(flex_binary PROPERTIES
    FOLDER "flex"
    POSITION_INDEPENDENT_CODE ON
)
