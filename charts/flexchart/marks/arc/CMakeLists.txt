# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/arc_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/arc_parser.y" "${LEMPAR}"
    COMMENT "Generating arc chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.c"
    COMMENT "Renaming arc parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/arc_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/arc_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.h"
    COMMENT "Generating arc chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.c"
    COMMENT "Renaming arc lexer .c → .cpp"
)

# Static library
add_library(flexchart_arc STATIC
    src/arc_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/arc_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/arc_parser_gen.cpp"
    src/arc_renderer.cpp
)
target_include_directories(flexchart_arc
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_arc PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_arc PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_arc_parser_test tests/test_arc_parser.cpp)
target_link_libraries(flexchart_arc_parser_test PRIVATE flexchart_arc TurboUtils::TinyTest)
add_test(NAME FlexChartArcParserTest COMMAND flexchart_arc_parser_test)
set_target_properties(flexchart_arc_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_arc_test tests/test_arc.cpp)
target_link_libraries(flexchart_arc_test PRIVATE flexchart_arc TurboUtils::TinyTest)
target_include_directories(flexchart_arc_test PRIVATE ../../src)
add_test(NAME FlexChartArcTest COMMAND flexchart_arc_test)
set_target_properties(flexchart_arc_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
