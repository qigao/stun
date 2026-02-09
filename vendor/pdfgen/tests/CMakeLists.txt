
# Find Catch2 (assuming it is provided by the parent project or available)
find_package(Catch2 CONFIG REQUIRED)

# Create the test executable
add_executable(pdfgen_tests 
    catch2_tests.cpp 
    rgb.c 
)

# Link against the main library and Catch2
target_link_libraries(pdfgen_tests PRIVATE pdfgen Catch2::Catch2WithMain)
target_include_directories(pdfgen_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Copy the data directory to the build directory so the test can find assets
add_custom_command(
    TARGET pdfgen_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../data
    $<TARGET_FILE_DIR:pdfgen_tests>/data
    COMMENT "Copying test data files..."
)

# Register the test with CTest
include(CTest)
add_test(NAME Catch2Tests COMMAND pdfgen_tests)
set_target_properties(pdfgen_tests PROPERTIES FOLDER "vendor/pdfgen")
