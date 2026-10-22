# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/tick_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/tick_parser.y" "${LEMPAR}"
    COMMENT "Generating tick chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.c"
    COMMENT "Renaming tick parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/tick_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/tick_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.h"
    COMMENT "Generating tick chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.c"
    COMMENT "Renaming tick lexer .c → .cpp"
)

# Static library
add_library(flexchart_tick STATIC
    src/tick_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/tick_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/tick_parser_gen.cpp"
    src/tick_renderer.cpp
)
target_include_directories(flexchart_tick
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_tick PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_tick PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_tick_parser_test tests/test_tick_parser.cpp)
target_link_libraries(flexchart_tick_parser_test PRIVATE flexchart_tick Salts::TinyTest)
add_test(NAME FlexChartTickParserTest COMMAND flexchart_tick_parser_test)
set_target_properties(flexchart_tick_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_tick_test tests/test_tick.cpp)
target_link_libraries(flexchart_tick_test PRIVATE flexchart_tick Salts::TinyTest)
target_include_directories(flexchart_tick_test PRIVATE ../../src)
add_test(NAME FlexChartTickTest COMMAND flexchart_tick_test)
set_target_properties(flexchart_tick_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
