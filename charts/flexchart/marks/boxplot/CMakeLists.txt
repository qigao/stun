# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/boxplot_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/boxplot_parser.y" "${LEMPAR}"
    COMMENT "Generating boxplot chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.c"
    COMMENT "Renaming boxplot parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/boxplot_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/boxplot_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.h"
    COMMENT "Generating boxplot chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.c"
    COMMENT "Renaming boxplot lexer .c → .cpp"
)

# Static library
add_library(flexchart_boxplot STATIC
    src/boxplot_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/boxplot_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/boxplot_parser_gen.cpp"
    src/boxplot_renderer.cpp
)
target_include_directories(flexchart_boxplot
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_boxplot PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_boxplot PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_boxplot_parser_test tests/test_boxplot_parser.cpp)
target_link_libraries(flexchart_boxplot_parser_test PRIVATE flexchart_boxplot TurboUtils::TinyTest)
add_test(NAME FlexChartBoxplotParserTest COMMAND flexchart_boxplot_parser_test)
set_target_properties(flexchart_boxplot_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_boxplot_test tests/test_boxplot.cpp)
target_link_libraries(flexchart_boxplot_test PRIVATE flexchart_boxplot TurboUtils::TinyTest)
target_include_directories(flexchart_boxplot_test PRIVATE ../../src)
add_test(NAME FlexChartBoxplotTest COMMAND flexchart_boxplot_test)
set_target_properties(flexchart_boxplot_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
