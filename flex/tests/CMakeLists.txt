# ============================================================================
# Test Suites - Link only required targets for faster compilation
# ============================================================================

find_package(Catch2 REQUIRED)
enable_testing()

# ----------------------------------------------------------------------------
# Compiler Tests (lexer/parser only)
# ----------------------------------------------------------------------------
add_executable(test_lexer_parser test_lexer_parser.cpp)
target_link_libraries(test_lexer_parser PRIVATE flex_compiler
                                                Catch2::Catch2WithMain )
target_include_directories(test_lexer_parser PRIVATE ../include)
set_target_properties(test_lexer_parser PROPERTIES FOLDER "flex/tests")
add_test(NAME test_lexer_parser COMMAND test_lexer_parser)

# ----------------------------------------------------------------------------
# Runtime Tests (runtime system only) - Clean version
# ----------------------------------------------------------------------------
add_executable(test_runtime test_runtime.cpp)
target_link_libraries(test_runtime PRIVATE flex_runtime Catch2::Catch2WithMain
                                           )
target_include_directories(test_runtime PRIVATE ../include)
set_target_properties(test_runtime PROPERTIES FOLDER "flex/tests")
add_test(NAME test_runtime COMMAND test_runtime)

# ----------------------------------------------------------------------------
# Layout Tests (requires full flex: parser + runtime)
# ----------------------------------------------------------------------------
add_executable(test_layout test_layout.cpp)
target_link_libraries(test_layout PRIVATE flex Catch2::Catch2WithMain )
target_include_directories(test_layout PRIVATE ../include)
set_target_properties(test_layout PROPERTIES FOLDER "flex/tests")
add_test(NAME test_layout COMMAND test_layout)

# ----------------------------------------------------------------------------
# Binary Format Tests (requires flex_binary + flex)
# ----------------------------------------------------------------------------
add_executable(test_binary test_binary.cpp)
target_link_libraries(test_binary PRIVATE flex_binary flex
                                          Catch2::Catch2WithMain )
target_include_directories(test_binary PRIVATE ../include)
set_target_properties(test_binary PROPERTIES FOLDER "flex/tests")
add_test(NAME test_binary COMMAND test_binary)
