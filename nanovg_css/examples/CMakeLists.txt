cmake_minimum_required(VERSION 3.15)

# Find SDL3
find_package(SDL3 REQUIRED)

# Find OpenGL
find_package(OpenGL REQUIRED)

# ============================================================================
# FlexUI Examples - Auto-generated from example_*.cpp files
# ============================================================================

# Find all example source files
file(GLOB EXAMPLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/example_*.cpp)

# Create an executable for each example
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    # Get the filename without directory and extension
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)

    # Special handling for legacy naming convention
    # Convert "flexui_example_*" to just "example_*" for consistency
    string(REPLACE "flexui_example_" "example_" EXAMPLE_TARGET ${EXAMPLE_NAME})

    # If it already starts with "example_", use as-is
    if(NOT ${EXAMPLE_NAME} MATCHES "^example_")
        set(EXAMPLE_TARGET "example_${EXAMPLE_NAME}")
    else()
        set(EXAMPLE_TARGET ${EXAMPLE_NAME})
    endif()

    message(STATUS "Adding FlexUI example: ${EXAMPLE_TARGET} (from ${EXAMPLE_NAME}.cpp)")

    # Create executable
    add_executable(${EXAMPLE_TARGET} ${EXAMPLE_SOURCE})

    # Link libraries
    target_link_libraries(${EXAMPLE_TARGET} PRIVATE
        SDL3::SDL3 nanovg_css
        OpenGL::GL
        glad::glad fmtlog
    )

    # Include directories
    target_include_directories(${EXAMPLE_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_BINARY_DIR}/../  # For flexui_resources.h
    )

    # Set target properties
    set_target_properties(${EXAMPLE_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        FOLDER nanovg_css/examples
    )
endforeach()

# ============================================================================
# Summary
# ============================================================================

list(LENGTH EXAMPLE_SOURCES EXAMPLE_COUNT)
message(STATUS "FlexUI: Configured ${EXAMPLE_COUNT} example(s)")
