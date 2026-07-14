# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/bar_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/bar_parser.y" "${LEMPAR}"
    COMMENT "Generating bar chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.c"
    COMMENT "Renaming bar parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/bar_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/bar_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.h"
    COMMENT "Generating bar chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.c"
    COMMENT "Renaming bar lexer .c → .cpp"
)

# Static library
add_library(flexchart_bar STATIC
    src/bar_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/bar_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/bar_parser_gen.cpp"
    src/bar_renderer.cpp
)
target_include_directories(flexchart_bar
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_bar PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_bar PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_bar_parser_test tests/test_bar_parser.cpp)
target_link_libraries(flexchart_bar_parser_test PRIVATE flexchart_bar TurboUtils::TinyTest)
add_test(NAME FlexChartBarParserTest COMMAND flexchart_bar_parser_test)
set_target_properties(flexchart_bar_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_bar_test tests/test_bar.cpp)
target_link_libraries(flexchart_bar_test PRIVATE flexchart_bar TurboUtils::TinyTest)
target_include_directories(flexchart_bar_test PRIVATE ../../src)
add_test(NAME FlexChartBarTest COMMAND flexchart_bar_test)
set_target_properties(flexchart_bar_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
