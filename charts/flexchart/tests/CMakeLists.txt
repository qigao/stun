# FlexChart Tests
cmake_minimum_required(VERSION 3.16)
 
    find_package(Catch2 REQUIRED)
    
    add_executable(flexchart_parser_tests
        chart_parser_tests.cpp
    )
    
    target_link_libraries(flexchart_parser_tests
        PRIVATE
            flexchart
            Catch2::Catch2WithMain
    )
    
    target_include_directories(flexchart_parser_tests
        PRIVATE
            ../include
            ../compiler
    )
    
    add_test(NAME FlexChartParserTests COMMAND flexchart_parser_tests)
    set_target_properties(flexchart_parser_tests PROPERTIES FOLDER "Modules/FlexChart/tests")
 
