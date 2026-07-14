# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/errorband_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/errorband_parser.y" "${LEMPAR}"
    COMMENT "Generating errorband chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.c"
    COMMENT "Renaming errorband parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/errorband_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/errorband_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.h"
    COMMENT "Generating errorband chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.c"
    COMMENT "Renaming errorband lexer .c → .cpp"
)

# Static library
add_library(flexchart_errorband STATIC
    src/errorband_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/errorband_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/errorband_parser_gen.cpp"
    src/errorband_renderer.cpp
)
target_include_directories(flexchart_errorband
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_errorband PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_errorband PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_errorband_parser_test tests/test_errorband_parser.cpp)
target_link_libraries(flexchart_errorband_parser_test PRIVATE flexchart_errorband TurboUtils::TinyTest)
add_test(NAME FlexChartErrorbandParserTest COMMAND flexchart_errorband_parser_test)
set_target_properties(flexchart_errorband_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_errorband_test tests/test_errorband.cpp)
target_link_libraries(flexchart_errorband_test PRIVATE flexchart_errorband TurboUtils::TinyTest)
target_include_directories(flexchart_errorband_test PRIVATE ../../src)
add_test(NAME FlexChartErrorbandTest COMMAND flexchart_errorband_test)
set_target_properties(flexchart_errorband_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
