# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/radar_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/radar_parser.y" "${LEMPAR}"
    COMMENT "Generating radar chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.c"
    COMMENT "Renaming radar parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/radar_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/radar_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.h"
    COMMENT "Generating radar chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.c"
    COMMENT "Renaming radar lexer .c → .cpp"
)

# Static library
add_library(flexchart_radar STATIC
    src/radar_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/radar_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/radar_parser_gen.cpp"
    src/radar_renderer.cpp
)
target_include_directories(flexchart_radar
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_radar PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_radar PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_radar_parser_test tests/test_radar_parser.cpp)
target_link_libraries(flexchart_radar_parser_test PRIVATE flexchart_radar TurboUtils::TinyTest)
add_test(NAME FlexChartRadarParserTest COMMAND flexchart_radar_parser_test)
set_target_properties(flexchart_radar_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_radar_test tests/test_radar.cpp)
target_link_libraries(flexchart_radar_test PRIVATE flexchart_radar TurboUtils::TinyTest)
target_include_directories(flexchart_radar_test PRIVATE ../../src)
add_test(NAME FlexChartRadarTest COMMAND flexchart_radar_test)
set_target_properties(flexchart_radar_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
