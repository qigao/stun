# ============================================================
# ThorVG Unit Tests
# ============================================================

enable_testing()

# Define resource directory
set(TEST_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

# ============================================================
# Test Sources
# ============================================================
set(TEST_SOURCES
    testAccessor.cpp
    testAnimation.cpp
    testFill.cpp
    testInitializer.cpp
    testLottie.cpp
    testMain.cpp
    testPaint.cpp
    testPicture.cpp
    testSavers.cpp
    testScene.cpp
    testShape.cpp
    testSwCanvas.cpp
    testSwEngine.cpp
    testText.cpp
)

# ============================================================
# Create Test Executable
# ============================================================
add_executable(tvgUnitTests ${TEST_SOURCES})

target_include_directories(tvgUnitTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_BINARY_DIR}/..
)

target_link_libraries(tvgUnitTests PRIVATE thorvg)

# Define TEST_DIR macro
target_compile_definitions(tvgUnitTests PRIVATE
    TEST_DIR="${TEST_RESOURCE_DIR}"
)

# Compiler flags
if(THORVG_BUILD_SHARED)
    # Shared library mode
else()
    target_compile_definitions(tvgUnitTests PRIVATE TVG_STATIC)
endif()

# Platform-specific dependencies
if(APPLE)
    target_link_libraries(tvgUnitTests PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
    )
endif()

# Set IDE folder
set_target_properties(tvgUnitTests PROPERTIES
    FOLDER "vendor/thorvg/tests"
)

# ============================================================
# Register Test with CTest
# ============================================================
add_test(NAME UnitTests COMMAND tvgUnitTests --success)

message(STATUS "ThorVG: Unit tests configured")
