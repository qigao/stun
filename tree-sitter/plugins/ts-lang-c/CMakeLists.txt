# Tree-sitter C Language Plugin

find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TREE_SITTER_C REQUIRED IMPORTED_TARGET tree-sitter-c)

add_library(ts-lang-c SHARED plugin.c)

target_include_directories(ts-lang-c PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts-lang-c PRIVATE 
    unofficial::tree-sitter::tree-sitter
    PkgConfig::TREE_SITTER_C
)

set_target_properties(ts-lang-c PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ts-lang-c"
    FOLDER "tree-sitter-plugins"
)


# Test
find_package(Catch2 REQUIRED)

add_executable(test_ts_lang_c test_parser.cpp plugin.c)
target_include_directories(test_ts_lang_c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(test_ts_lang_c PRIVATE 
    unofficial::tree-sitter::tree-sitter
    PkgConfig::TREE_SITTER_C
    Catch2::Catch2WithMain
)
set_target_properties(test_ts_lang_c PROPERTIES FOLDER "tree-sitter-plugins/tests")
add_test(NAME test_ts_lang_c COMMAND test_ts_lang_c)
