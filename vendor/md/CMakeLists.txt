set(TARGET_NAME md_parser)
find_program(RE2C_EXECUTABLE re2c)
find_program(LEMON_EXECUTABLE lemon)

if(NOT RE2C_EXECUTABLE OR NOT LEMON_EXECUTABLE)
    message(WARNING "md: re2c or lemon not found. Skipping build.")
    return()
endif()

set(LEMPAR ${CMAKE_SOURCE_DIR}/vendor/lemon/lempar.c)

# Generate parser
set(MD_PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/md_parser.y)
set(MD_PARSER_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/md_parser_gen.c)
set(MD_PARSER_GEN ${CMAKE_CURRENT_BINARY_DIR}/md_parser_gen.cpp)
set(MD_PARSER_H ${CMAKE_CURRENT_BINARY_DIR}/md_parser_gen.h)

add_custom_command(
  OUTPUT ${MD_PARSER_GEN} ${MD_PARSER_H}
  COMMAND ${CMAKE_COMMAND} -E copy ${MD_PARSER_Y} ${CMAKE_CURRENT_BINARY_DIR}/md_parser_gen.y
  COMMAND ${LEMON_EXECUTABLE} -T${LEMPAR} ${CMAKE_CURRENT_BINARY_DIR}/md_parser_gen.y
  COMMAND ${CMAKE_COMMAND} -E rename ${MD_PARSER_GEN_C} ${MD_PARSER_GEN}
  DEPENDS ${MD_PARSER_Y}
  VERBATIM)

# Generate lexer
set(MD_LEXER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/md_lexer.re)
set(MD_LEXER_GEN ${CMAKE_CURRENT_BINARY_DIR}/md_lexer_gen.cpp)

add_custom_command(
  OUTPUT ${MD_LEXER_GEN}
  COMMAND ${RE2C_EXECUTABLE} -o ${MD_LEXER_GEN} ${MD_LEXER_RE}
  DEPENDS ${MD_LEXER_RE} ${MD_PARSER_H}
  VERBATIM)

add_library(${TARGET_NAME} STATIC 
    ${MD_PARSER_GEN} 
    ${MD_LEXER_GEN} 
    src/md_parser_driver.cpp
    src/md_inline_processor.cpp
    src/md_block_processor.cpp
)

target_include_directories(${TARGET_NAME} 
    PUBLIC include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${TARGET_NAME} PUBLIC TurboNet::Utils    )
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "md")
# ============================================================================
# Unit Tests
# ============================================================================
if(BUILD_TESTS)
    add_subdirectory(test)
endif()
