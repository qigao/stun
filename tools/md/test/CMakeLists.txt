
# ============================================================================
# Unit Tests
# ============================================================================
find_package(Catch2 REQUIRED)

add_executable(test_md_re2c test_md_re2c.cpp)
target_link_libraries(test_md_re2c PRIVATE ${TARGET_NAME} Catch2::Catch2WithMain)
target_include_directories(test_md_re2c PRIVATE ../include)
set_target_properties(test_md_re2c PROPERTIES FOLDER "md/tests")

add_test(NAME test_md_re2c COMMAND test_md_re2c)

add_executable(indent_test indent_test.cpp)
target_link_libraries(indent_test PRIVATE ${TARGET_NAME})
target_include_directories(indent_test PRIVATE ../include)
set_target_properties(indent_test PROPERTIES FOLDER "md/tests")

add_executable(inline_test inline_test.cpp)
target_link_libraries(inline_test PRIVATE ${TARGET_NAME})
target_include_directories(inline_test PRIVATE ../include)
set_target_properties(inline_test PROPERTIES FOLDER "md/tests")

add_executable(structure_test structure_test.cpp)
target_link_libraries(structure_test PRIVATE ${TARGET_NAME})
target_include_directories(structure_test PRIVATE ../include)
set_target_properties(structure_test PROPERTIES FOLDER "md/tests")

# Spec test runner - generic
add_executable(spec_runner spec_runner.cpp)
target_link_libraries(spec_runner PRIVATE ${TARGET_NAME})
target_include_directories(spec_runner PRIVATE ../include)
set_target_properties(spec_runner PROPERTIES FOLDER "md/tests")

# Spec tests automatically detected
file(GLOB SPEC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/spec-*.txt" "${CMAKE_CURRENT_SOURCE_DIR}/spec.txt")
foreach(SPEC_PATH ${SPEC_FILES})
    get_filename_component(SPEC_NAME ${SPEC_PATH} NAME_WE)
    string(REPLACE "spec-" "" NAME_PART ${SPEC_NAME})
    string(REPLACE "-" "_" NAME_UNDERSCORE ${NAME_PART})
    set(TEST_TARGET_NAME "spec_test_${NAME_UNDERSCORE}")
    
    add_executable(${TEST_TARGET_NAME} spec_runner.cpp)
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE ${TARGET_NAME})
    target_include_directories(${TEST_TARGET_NAME} PRIVATE ../include)
    target_compile_definitions(${TEST_TARGET_NAME} PRIVATE SPEC_FILE="${SPEC_PATH}")
    set_target_properties(${TEST_TARGET_NAME} PROPERTIES FOLDER "tools/md/tests")
endforeach()
