# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rule_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/rule_parser.y" "${LEMPAR}"
    COMMENT "Generating rule chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.c"
    COMMENT "Renaming rule parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rule_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/rule_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.h"
    COMMENT "Generating rule chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.c"
    COMMENT "Renaming rule lexer .c → .cpp"
)

# Static library
add_library(flexchart_rule STATIC
    src/rule_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/rule_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/rule_parser_gen.cpp"
    src/rule_renderer.cpp
)
target_include_directories(flexchart_rule
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_rule PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_rule PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_rule_parser_test tests/test_rule_parser.cpp)
target_link_libraries(flexchart_rule_parser_test PRIVATE flexchart_rule TurboUtils::TinyTest)
add_test(NAME FlexChartRuleParserTest COMMAND flexchart_rule_parser_test)
set_target_properties(flexchart_rule_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_rule_test tests/test_rule.cpp)
target_link_libraries(flexchart_rule_test PRIVATE flexchart_rule TurboUtils::TinyTest)
target_include_directories(flexchart_rule_test PRIVATE ../../src)
add_test(NAME FlexChartRuleTest COMMAND flexchart_rule_test)
set_target_properties(flexchart_rule_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
