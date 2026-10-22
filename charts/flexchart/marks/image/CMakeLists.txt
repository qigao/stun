# Parser generation (lemon → .c, then rename to .cpp for C++ compilation)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.c"
           "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.h"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/src/image_parser.y"
            "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.y"
    COMMAND ${LEMON_EXECUTABLE} -q -d"${CMAKE_CURRENT_BINARY_DIR}"
            -T"${LEMPAR}" "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.y"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/image_parser.y" "${LEMPAR}"
    COMMENT "Generating image chart parser with lemon"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.c"
    COMMENT "Renaming image parser .c → .cpp"
)

# Lexer generation (re2c → .c, then rename to .cpp)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.c"
    COMMAND ${RE2C_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/image_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/image_lexer.re"
            "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.h"
    COMMENT "Generating image chart lexer with re2c"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.cpp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.c"
            "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.cpp"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.c"
    COMMENT "Renaming image lexer .c → .cpp"
)

# Static library
add_library(flexchart_image STATIC
    src/image_parser_wrapper.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/image_lexer_gen.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/image_parser_gen.cpp"
    src/image_renderer.cpp
)
target_include_directories(flexchart_image
    PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE ../../src
)
target_link_libraries(flexchart_image PUBLIC flexchart_common flexchart)
set_target_properties(flexchart_image PROPERTIES FOLDER "Modules/FlexChart/Marks")

# Parser test
add_executable(flexchart_image_parser_test tests/test_image_parser.cpp)
target_link_libraries(flexchart_image_parser_test PRIVATE flexchart_image Salts::TinyTest)
add_test(NAME FlexChartImageParserTest COMMAND flexchart_image_parser_test)
set_target_properties(flexchart_image_parser_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")

# Renderer test
add_executable(flexchart_image_test tests/test_image.cpp)
target_link_libraries(flexchart_image_test PRIVATE flexchart_image Salts::TinyTest)
target_include_directories(flexchart_image_test PRIVATE ../../src)
add_test(NAME FlexChartImageTest COMMAND flexchart_image_test)
set_target_properties(flexchart_image_test PROPERTIES FOLDER "Modules/FlexChart/Marks/tests")
