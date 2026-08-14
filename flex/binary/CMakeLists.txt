# Flex Binary Format Library
cmake_minimum_required(VERSION 3.16)

# Find zstd for compression
find_package(zstd CONFIG REQUIRED)

# ============================================================================
# flex_binary - Binary serialization library
# ============================================================================

add_library(flex_binary STATIC
    src/format.cpp
    src/writer.cpp
    src/reader.cpp
    src/compiler.cpp
    src/definition_binary.cpp
)

target_include_directories(flex_binary
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${FLEX_ENGINE_ROOT}/modules/engine
)

target_link_libraries(flex_binary
    PUBLIC Flex::Engine Flex::Compiler Flex::Runtime
    PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
)

set_target_properties(flex_binary PROPERTIES
    FOLDER "flex/binary"
    POSITION_INDEPENDENT_CODE ON
)

add_library(Flex::Binary ALIAS flex_binary)
