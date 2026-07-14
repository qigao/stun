# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/line_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/line_parser.y" "${LEMPAR}"
    COMMENT "Generating line chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.c"
    COMMENT "Renaming line parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/line_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/line_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.h"
    COMMENT "Generating line chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.c"
    COMMENT "Renaming line lexer .c → .cpp"
)

# Static library
add_library(flexchart_line STATIC
    src/line_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/line_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/line_parser_gen.cpp"
    src/line_renderer.cpp
)
target_include_directories(flexchart_line
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_line PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_line PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_line_parser_test tests/test_line_parser.cpp)
target_link_libraries(flexchart_line_parser_test PRIVATE flexchart_line TurboUtils::TinyTest)
add_test(NAME FlexChartLineParserTest COMMAND flexchart_line_parser_test)
set_target_properties(flexchart_line_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_line_test tests/test_line.cpp)
target_link_libraries(flexchart_line_test PRIVATE flexchart_line TurboUtils::TinyTest)
target_include_directories(flexchart_line_test PRIVATE ../../src)
add_test(NAME FlexChartLineTest COMMAND flexchart_line_test)
set_target_properties(flexchart_line_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
