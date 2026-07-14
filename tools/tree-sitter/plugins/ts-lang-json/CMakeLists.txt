# Tree-sitter JSON Language Plugin

find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH}
    "${EXTERNAL_ROOT}/tree-sitter/json/share/pkgconfig;$ENV{PKG_CONFIG_PATH}")
pkg_check_modules(TREE_SITTER_JSON REQUIRED IMPORTED_TARGET tree-sitter-json)

add_library(ts-lang-json SHARED plugin.c)

target_include_directories(ts-lang-json PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ts-lang-json PRIVATE unofficial::tree-sitter::tree-sitter
                                           PkgConfig::TREE_SITTER_JSON)

set_target_properties(
  ts-lang-json
  PROPERTIES PREFIX ""
             OUTPUT_NAME "ts-lang-json"
             FOLDER "tools/tree-sitter")

# Test
find_package(Catch2 REQUIRED)

add_executable(test_ts_lang_json test_parser.cpp plugin.c)
target_include_directories(test_ts_lang_json
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(
  test_ts_lang_json PRIVATE unofficial::tree-sitter::tree-sitter
                            PkgConfig::TREE_SITTER_JSON Catch2::Catch2WithMain)
set_target_properties(test_ts_lang_json PROPERTIES FOLDER "tools/tree-sitter")
add_test(NAME test_ts_lang_json COMMAND test_ts_lang_json)

add_executable(test_ts_lang_json_bdd test_bdd.c plugin.c)
target_include_directories(test_ts_lang_json_bdd
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(
  test_ts_lang_json_bdd PRIVATE unofficial::tree-sitter::tree-sitter
                                PkgConfig::TREE_SITTER_JSON)
set_target_properties(test_ts_lang_json_bdd PROPERTIES FOLDER
                                                       "tools/tree-sitter")
add_test(NAME test_ts_lang_json_bdd COMMAND test_ts_lang_json_bdd)
