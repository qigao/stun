/*
 * Unit tests for tree-sitter C language plugin
 */

#include <catch2/catch_all.hpp>
#include "../ts_plugin.h"
#include <cstring>

extern "C" const TSPluginInfo* ts_plugin_info(void);

TEST_CASE("Plugin info is valid", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    
    REQUIRE(info != nullptr);
    REQUIRE(std::string(info->name) == "c");
    REQUIRE(info->highlight != nullptr);
    REQUIRE(info->free_result != nullptr);
}

TEST_CASE("Highlight empty string", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    
    TSHighlightResult* result = info->highlight("", 0);
    REQUIRE(result != nullptr);
    REQUIRE(result->count == 0);
    
    info->free_result(result);
}

TEST_CASE("Highlight preprocessor directive", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "#include <stdio.h>";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    REQUIRE(result->count > 0);
    
    bool found_preproc = false;
    bool found_string = false;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_PREPROCESSOR) found_preproc = true;
        if (result->tokens[i].type == TS_HIGHLIGHT_STRING) found_string = true;
    }
    
    CHECK(found_preproc);
    CHECK(found_string);
    
    info->free_result(result);
}

TEST_CASE("Highlight types", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "int x; void* p; char c;";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    
    int type_count = 0;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_TYPE) type_count++;
    }
    
    CHECK(type_count >= 3);
    
    info->free_result(result);
}

TEST_CASE("Highlight numbers", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "int x = 42; float y = 3.14;";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    
    int num_count = 0;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_NUMBER) num_count++;
    }
    
    CHECK(num_count >= 2);
    
    info->free_result(result);
}

TEST_CASE("Highlight strings", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "char* s = \"hello\"; char c = 'x';";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    
    int str_count = 0;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_STRING) str_count++;
    }
    
    CHECK(str_count >= 2);
    
    info->free_result(result);
}

TEST_CASE("Highlight keywords", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "if (x) { return 1; } else { return 0; }";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    
    int kw_count = 0;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_KEYWORD) kw_count++;
    }
    
    CHECK(kw_count >= 3);  // if, return, else, return
    
    info->free_result(result);
}

TEST_CASE("Highlight comments", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = "int x; // line comment\nint y; /* block */";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    
    int comment_count = 0;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_COMMENT) comment_count++;
    }
    
    CHECK(comment_count >= 2);
    
    info->free_result(result);
}

TEST_CASE("Full function highlighting", "[ts-lang-c]") {
    const TSPluginInfo* info = ts_plugin_info();
    const char* code = R"(
int main(void) {
    int x = 42;
    if (x > 0) {
        return 1;
    }
    return 0;
}
)";
    
    TSHighlightResult* result = info->highlight(code, strlen(code));
    REQUIRE(result != nullptr);
    REQUIRE(result->count > 0);
    
    bool has_type = false, has_number = false, has_keyword = false;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->tokens[i].type == TS_HIGHLIGHT_TYPE) has_type = true;
        if (result->tokens[i].type == TS_HIGHLIGHT_NUMBER) has_number = true;
        if (result->tokens[i].type == TS_HIGHLIGHT_KEYWORD) has_keyword = true;
    }
    
    CHECK(has_type);
    CHECK(has_number);
    CHECK(has_keyword);
    
    info->free_result(result);
}
