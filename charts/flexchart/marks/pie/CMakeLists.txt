# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pie_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/pie_parser.y" "${LEMPAR}"
    COMMENT "Generating pie chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.c"
    COMMENT "Renaming pie parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pie_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/pie_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.h"
    COMMENT "Generating pie chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.c"
    COMMENT "Renaming pie lexer .c → .cpp"
)

# Static library
add_library(flexchart_pie STATIC
    src/pie_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/pie_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/pie_parser_gen.cpp"
    src/pie_renderer.cpp
)
target_include_directories(flexchart_pie
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_pie PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_pie PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_pie_parser_test tests/test_pie_parser.cpp)
target_link_libraries(flexchart_pie_parser_test PRIVATE flexchart_pie Salts::TinyTest)
add_test(NAME FlexChartPieParserTest COMMAND flexchart_pie_parser_test)
set_target_properties(flexchart_pie_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_pie_test tests/test_pie.cpp)
target_link_libraries(flexchart_pie_test PRIVATE flexchart_pie Salts::TinyTest)
target_include_directories(flexchart_pie_test PRIVATE ../../src)
add_test(NAME FlexChartPieTest COMMAND flexchart_pie_test)
set_target_properties(flexchart_pie_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
