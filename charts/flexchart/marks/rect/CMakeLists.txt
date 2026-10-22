# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rect_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/rect_parser.y" "${LEMPAR}"
    COMMENT "Generating rect chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.c"
    COMMENT "Renaming rect parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rect_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/rect_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.h"
    COMMENT "Generating rect chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.c"
    COMMENT "Renaming rect lexer .c → .cpp"
)

# Static library
add_library(flexchart_rect STATIC
    src/rect_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/rect_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/rect_parser_gen.cpp"
    src/rect_renderer.cpp
)
target_include_directories(flexchart_rect
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_rect PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_rect PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_rect_parser_test tests/test_rect_parser.cpp)
target_link_libraries(flexchart_rect_parser_test PRIVATE flexchart_rect Salts::TinyTest)
add_test(NAME FlexChartRectParserTest COMMAND flexchart_rect_parser_test)
set_target_properties(flexchart_rect_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_rect_test tests/test_rect.cpp)
target_link_libraries(flexchart_rect_test PRIVATE flexchart_rect Salts::TinyTest)
target_include_directories(flexchart_rect_test PRIVATE ../../src)
add_test(NAME FlexChartRectTest COMMAND flexchart_rect_test)
set_target_properties(flexchart_rect_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
