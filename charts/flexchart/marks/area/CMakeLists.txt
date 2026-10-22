# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/area_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/area_parser.y" "${LEMPAR}"
    COMMENT "Generating area chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.c"
    COMMENT "Renaming area parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/area_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/area_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.h"
    COMMENT "Generating area chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.c"
    COMMENT "Renaming area lexer .c → .cpp"
)

# Static library
add_library(flexchart_area STATIC
    src/area_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/area_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/area_parser_gen.cpp"
    src/area_renderer.cpp
)
target_include_directories(flexchart_area
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_area PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_area PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_area_parser_test tests/test_area_parser.cpp)
target_link_libraries(flexchart_area_parser_test PRIVATE flexchart_area Salts::TinyTest)
add_test(NAME FlexChartAreaParserTest COMMAND flexchart_area_parser_test)
set_target_properties(flexchart_area_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_area_test tests/test_area.cpp)
target_link_libraries(flexchart_area_test PRIVATE flexchart_area Salts::TinyTest)
target_include_directories(flexchart_area_test PRIVATE ../../src)
add_test(NAME FlexChartAreaTest COMMAND flexchart_area_test)
set_target_properties(flexchart_area_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
