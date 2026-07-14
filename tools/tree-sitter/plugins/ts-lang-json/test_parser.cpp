/*
 * Unit tests for tree-sitter JSON language plugin
 */

#include <catch2/catch_all.hpp>
#include "../ts_plugin.h"
#include <cstring>

extern "C" const TSPluginInfo* ts_plugin_info(void);

TEST_CASE("JSON Plugin info is valid", "[ts-lang-json]") {
    const TSPluginInfo* info = ts_plugin_info();
    
    REQUIRE(info != nullptr);
    REQUIRE(std::string(info->name) == "json");
    REQUIRE(info->highlight != nullptr);
    REQUIRE(info->free_result != nullptr);
}

TEST_CASE("JSON Highlight empty string", "[ts-lang-json]") {
    const TSPluginInfo* info = ts_plugin_info();
    
    TSHighlightResult* result = info->highlight("", 0);
    REQUIRE(result != nullptr);
    REQUIRE(result->count == 0);
    
    info->free_result(result);
}

TEST_CASE("JSON Highlight basic object", "[ts-lang-json]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "{\"key\": \"value\", \"num\": 42, \"bool\": true, \"null\": null}";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    REQUIRE(result->count > 0);
    
    int key_count = 0;
    int str_count = 0;
    int num_count = 0;
    int const_count = 0;
    
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_VARIABLE) key_count++;
        if (result->tokens[i].type == TS_HIGHLIGHT_STRING) str_count++;
        if (result->tokens[i].type == TS_HIGHLIGHT_NUMBER) num_count++;
        if (result->tokens[i].type == TS_HIGHLIGHT_CONSTANT) const_count++;
    }
    
    // keys: "key", "num", "bool", "null" (4)
    // values: "value" (1 string)
    // 42 (1 number)
    // true, null (2 constants)
    
    CHECK(key_count == 4);
    CHECK(str_count == 1);
    CHECK(num_count == 1);
    CHECK(const_count == 2);
    
    info->free_result(result);
}

TEST_CASE("JSON Highlight nested structure", "[ts-lang-json]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "{\"a\": [1, 2, {\"b\": 3}]}";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    
    int key_count = 0;
    int num_count = 0;
    
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_VARIABLE) key_count++;
        if (result->tokens[i].type == TS_HIGHLIGHT_NUMBER) num_count++;
    }
    
    CHECK(key_count == 2); // "a", "b"
    CHECK(num_count == 3); // 1, 2, 3
    
    info->free_result(result);
}
