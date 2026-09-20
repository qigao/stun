# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/geoshape_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/geoshape_parser.y" "${LEMPAR}"
    COMMENT "Generating geoshape chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.c"
    COMMENT "Renaming geoshape parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/geoshape_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/geoshape_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.h"
    COMMENT "Generating geoshape chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.c"
    COMMENT "Renaming geoshape lexer .c → .cpp"
)

# Static library
add_library(flexchart_geoshape STATIC
    src/geoshape_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/geoshape_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/geoshape_parser_gen.cpp"
    src/geoshape_renderer.cpp
)
target_include_directories(flexchart_geoshape
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_geoshape PUBLIC flexchart_common flexchart PRIVATE TurboParser::Parser)
set_target_properties(flexchart_geoshape PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_geoshape_parser_test tests/test_geoshape_parser.cpp)
target_link_libraries(flexchart_geoshape_parser_test PRIVATE flexchart_geoshape Salts::TinyTest)
add_test(NAME FlexChartGeoshapeParserTest COMMAND flexchart_geoshape_parser_test)
set_target_properties(flexchart_geoshape_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_geoshape_test tests/test_geoshape.cpp)
target_link_libraries(flexchart_geoshape_test PRIVATE flexchart_geoshape Salts::TinyTest)
target_include_directories(flexchart_geoshape_test PRIVATE ../../src)
add_test(NAME FlexChartGeoshapeTest COMMAND flexchart_geoshape_test)
set_target_properties(flexchart_geoshape_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
