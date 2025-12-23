 # ============================================================================
# All test suites (unified configuration)
# ============================================================================
set(ALL_TESTS
    test_lexer_parser
    test_runtime
    test_layout
)
find_package(Catch2)
enable_testing()
foreach(test_name ${ALL_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE flex Catch2::Catch2WithMain fmtlog)
    target_include_directories(${test_name} PRIVATE ../include) 
    set_target_properties(${test_name} PROPERTIES FOLDER flex/tests/)
     add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()