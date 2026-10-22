add_executable(tango_core_test tango_core_test.c)
target_link_libraries(tango_core_test PRIVATE tango)
add_test(NAME tango_core_test COMMAND tango_core_test)
