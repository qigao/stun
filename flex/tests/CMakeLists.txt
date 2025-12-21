# Flex Tests with Catch2

find_package(Catch2 3 QUIET)

add_executable(test_flex test_flex.cpp)
target_link_libraries(test_flex PRIVATE flex Catch2::Catch2WithMain)
set_target_properties(test_flex PROPERTIES FOLDER "flex/tests")

# Register with CTest
include(Catch)
catch_discover_tests(test_flex)
