# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/trail_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/trail_parser.y" "${LEMPAR}"
    COMMENT "Generating trail chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.c"
    COMMENT "Renaming trail parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/trail_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/trail_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.h"
    COMMENT "Generating trail chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.c"
    COMMENT "Renaming trail lexer .c → .cpp"
)

# Static library
add_library(flexchart_trail STATIC
    src/trail_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/trail_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/trail_parser_gen.cpp"
    src/trail_renderer.cpp
)
target_include_directories(flexchart_trail
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_trail PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_trail PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_trail_parser_test tests/test_trail_parser.cpp)
target_link_libraries(flexchart_trail_parser_test PRIVATE flexchart_trail Salts::TinyTest)
add_test(NAME FlexChartTrailParserTest COMMAND flexchart_trail_parser_test)
set_target_properties(flexchart_trail_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_trail_test tests/test_trail.cpp)
target_link_libraries(flexchart_trail_test PRIVATE flexchart_trail Salts::TinyTest)
target_include_directories(flexchart_trail_test PRIVATE ../../src)
add_test(NAME FlexChartTrailTest COMMAND flexchart_trail_test)
set_target_properties(flexchart_trail_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
