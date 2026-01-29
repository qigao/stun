project(md_re2c LANGUAGES C CXX)

find_program(RE2C_EXECUTABLE re2c)
find_program(LEMON_EXECUTABLE lemon)

if(NOT RE2C_EXECUTABLE OR NOT LEMON_EXECUTABLE)
    message(WARNING "md-re2c: re2c or lemon not found. Skipping build.")
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

add_library(md_re2c STATIC 
    ${MD_PARSER_GEN} 
    ${MD_LEXER_GEN} 
    src/md_parser_driver.cpp
    src/md_inline_processor.cpp
    src/md_block_processor.cpp
)

target_include_directories(md_re2c 
    PUBLIC include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(md_re2c PUBLIC TurboNet::Memory PRIVATE fmtlog)
set_target_properties(md_re2c PROPERTIES FOLDER "md-re2c")
# ============================================================================
# Unit Tests
# ============================================================================
find_package(Catch2 REQUIRED)

add_executable(test_md_re2c test/test_md_re2c.cpp)
target_link_libraries(test_md_re2c PRIVATE md_re2c Catch2::Catch2WithMain)
target_include_directories(test_md_re2c PRIVATE include)
set_target_properties(test_md_re2c PROPERTIES FOLDER "md-re2c/tests")

add_test(NAME test_md_re2c COMMAND test_md_re2c)

# Debug test executables
add_executable(debug_test test/debug_test.cpp)
target_link_libraries(debug_test PRIVATE md_re2c)
target_include_directories(debug_test PRIVATE include)
set_target_properties(debug_test PROPERTIES FOLDER "md-re2c/tests")

add_executable(lexer_test test/lexer_test.cpp)
target_link_libraries(lexer_test PRIVATE md_re2c)
target_include_directories(lexer_test PRIVATE include ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(lexer_test PROPERTIES FOLDER "md-re2c/tests")

add_executable(indent_test test/indent_test.cpp)
target_link_libraries(indent_test PRIVATE md_re2c)
target_include_directories(indent_test PRIVATE include)
set_target_properties(indent_test PROPERTIES FOLDER "md-re2c/tests")

add_executable(inline_test test/inline_test.cpp)
target_link_libraries(inline_test PRIVATE md_re2c)
target_include_directories(inline_test PRIVATE include)
set_target_properties(inline_test PROPERTIES FOLDER "md-re2c/tests")

add_executable(structure_test test/debug_test.cpp)
target_link_libraries(structure_test PRIVATE md_re2c)
target_include_directories(structure_test PRIVATE include)
set_target_properties(structure_test PROPERTIES FOLDER "md-re2c/tests")

# Spec test runner - generic
add_executable(spec_runner test/spec_runner.cpp)
target_link_libraries(spec_runner PRIVATE md_re2c)
target_include_directories(spec_runner PRIVATE include)
set_target_properties(spec_runner PROPERTIES FOLDER "md-re2c/tests")

# Spec tests automatically detected
file(GLOB SPEC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test/spec-*.txt" "${CMAKE_CURRENT_SOURCE_DIR}/test/spec.txt")
foreach(SPEC_PATH ${SPEC_FILES})
    get_filename_component(SPEC_NAME ${SPEC_PATH} NAME_WE)
    string(REPLACE "spec-" "" NAME_PART ${SPEC_NAME})
    string(REPLACE "-" "_" NAME_UNDERSCORE ${NAME_PART})
    set(TARGET_NAME "spec_test_${NAME_UNDERSCORE}")
    
    add_executable(${TARGET_NAME} test/spec_runner.cpp)
    target_link_libraries(${TARGET_NAME} PRIVATE md_re2c)
    target_include_directories(${TARGET_NAME} PRIVATE include)
    target_compile_definitions(${TARGET_NAME} PRIVATE SPEC_FILE="${SPEC_PATH}")
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "md-re2c/tests")
endforeach()
