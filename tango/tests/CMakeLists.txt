add_executable(tango_core_test tango_core_test.c)
target_link_libraries(tango_core_test PRIVATE tango)
target_include_directories(tango_core_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME tango_core_test COMMAND tango_core_test)
