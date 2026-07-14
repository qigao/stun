# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/point_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/point_parser.y" "${LEMPAR}"
    COMMENT "Generating point chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.c"
    COMMENT "Renaming point parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/point_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/point_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.h"
    COMMENT "Generating point chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.c"
    COMMENT "Renaming point lexer .c → .cpp"
)

# Static library
add_library(flexchart_point STATIC
    src/point_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/point_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/point_parser_gen.cpp"
    src/point_renderer.cpp
)
target_include_directories(flexchart_point
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_point PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_point PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_point_parser_test tests/test_point_parser.cpp)
target_link_libraries(flexchart_point_parser_test PRIVATE flexchart_point TurboUtils::TinyTest)
add_test(NAME FlexChartPointParserTest COMMAND flexchart_point_parser_test)
set_target_properties(flexchart_point_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_point_test tests/test_point.cpp)
target_link_libraries(flexchart_point_test PRIVATE flexchart_point TurboUtils::TinyTest)
target_include_directories(flexchart_point_test PRIVATE ../../src)
add_test(NAME FlexChartPointTest COMMAND flexchart_point_test)
set_target_properties(flexchart_point_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
