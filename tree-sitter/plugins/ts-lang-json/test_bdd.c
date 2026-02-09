#include "../../../vendor/bdd/bdd-for-c.h"
#include "../ts_plugin.h"
#include <string.h>

extern const TSPluginInfo* ts_plugin_info(void);

spec("ts-lang-json") {
    static const TSPluginInfo* info = NULL;

    before() {
        info = ts_plugin_info();
    }

    describe("Plugin Information") {
        it("should have the correct name") {
            check(strcmp(info->name, "json") == 0);
        }

        it("should have a version") {
            check(info->version != NULL);
            check(strlen(info->version) > 0);
        }

        it("should export highlight and free_result functions") {
            check(info->highlight != NULL);
            check(info->free_result != NULL);
        }
    }

    describe("Highlighting") {
        it("should handle empty strings") {
            TSHighlightResult* result = info->highlight("", 0);
            check(result != NULL);
            check(result->count == 0);
            info->free_result(result);
        }

        it("should highlight a simple object") {
            const char* code = "{\"foo\": 123}";
            TSHighlightResult* result = info->highlight(code, strlen(code));
            
            check(result != NULL);
            check(result->count >= 2);
            
            bool found_key = false;
            bool found_number = false;
            
            for (uint32_t i = 0; i < result->count; i++) {
                if (result->tokens[i].type == TS_HIGHLIGHT_VARIABLE) found_key = true;
                if (result->tokens[i].type == TS_HIGHLIGHT_NUMBER) found_number = true;
            }
            
            check(found_key);
            check(found_number);
            
            info->free_result(result);
        }

        it("should highlight boolean constants") {
            const char* code = "[true, false, null]";
            TSHighlightResult* result = info->highlight(code, strlen(code));
            
            check(result != NULL);
            int const_count = 0;
            for (uint32_t i = 0; i < result->count; i++) {
                if (result->tokens[i].type == TS_HIGHLIGHT_CONSTANT) const_count++;
            }
            
            check(const_count == 3);
            info->free_result(result);
        }
    }
}
