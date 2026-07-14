# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/text_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/text_parser.y" "${LEMPAR}"
    COMMENT "Generating text chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.c"
    COMMENT "Renaming text parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/text_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/text_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.h"
    COMMENT "Generating text chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.c"
    COMMENT "Renaming text lexer .c → .cpp"
)

# Static library
add_library(flexchart_text STATIC
    src/text_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/text_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/text_parser_gen.cpp"
    src/text_renderer.cpp
)
target_include_directories(flexchart_text
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_text PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_text PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_text_parser_test tests/test_text_parser.cpp)
target_link_libraries(flexchart_text_parser_test PRIVATE flexchart_text TurboUtils::TinyTest)
add_test(NAME FlexChartTextParserTest COMMAND flexchart_text_parser_test)
set_target_properties(flexchart_text_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_text_test tests/test_text.cpp)
target_link_libraries(flexchart_text_test PRIVATE flexchart_text TurboUtils::TinyTest)
target_include_directories(flexchart_text_test PRIVATE ../../src)
add_test(NAME FlexChartTextTest COMMAND flexchart_text_test)
set_target_properties(flexchart_text_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
