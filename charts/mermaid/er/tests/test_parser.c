#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "er/er_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

extern ERDiagram* er_parse(const char* input);
extern void er_free(ERDiagram* diagram);
extern char* er_to_json(ERDiagram* diagram);

// --- Helpers ---

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    
    char* buf = (char*)malloc(size + 1);
    if (fread(buf, 1, size, f) != (size_t)size) {
        free(buf); fclose(f); return NULL; 
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static char* load_golden_file(const char* case_name, const char* suffix, const char* ext) {
    char path[512];
    if (suffix && strlen(suffix) > 0) {
        snprintf(path, sizeof(path), "%s/%s.%s.%s", GOLDEN_DIR, case_name, suffix, ext);
    } else {
        snprintf(path, sizeof(path), "%s/%s.%s", GOLDEN_DIR, case_name, ext);
    }
    return read_file(path);
}

// Simple JSON normalization (not sorting keys, just basic structural equality check via serialization?)
// Or rely on turbo_parser's deterministic serialization if key order matches.
// Since we don't have a reliable json normalizer handy, we use string comparison for now, 
// OR we can implement a weak comparison. 
// Flowchart test implemented json_equal by re-parsing and re-serializing.

static void normalize_json(json_value_t* val) {
    if (!val) return;
    // Implementation skipped for brevity, relying on consistent serialization order
    // But turbo_parser *usually* preserves add order.
    // Ideally we should sort keys.
}

static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (turbo_parse_json((const uint8_t*)a, strlen(a), &ja) != 0) return 0;
    if (turbo_parse_json((const uint8_t*)b, strlen(b), &jb) != 0) {
        turbo_free_json(&ja);
        return 0;
    }
    
    // Normalization logic would go here
    // For now we assume consistent serialization order

    char *sa = turbo_json_serialize(ja, NULL);
    char *sb = turbo_json_serialize(jb, NULL);
    int eq = (sa && sb && strcmp(sa, sb) == 0);
    turbo_json_serialize_free(sa);
    turbo_json_serialize_free(sb);
    turbo_free_json(&ja);
    turbo_free_json(&jb);
    return eq;
}

spec("er_parser") {
    describe("parsing basics") {
        it("should sanity check manual input") {
            const char* input = "erDiagram\n CUSTOMER ||--o{ ORDER : places";
            ERDiagram* d = er_parse(input);
            REQUIRE(d != NULL);
            er_free(d);
        }
    }

    describe("golden tests") {
#ifdef _WIN32
        char search_path[MAX_PATH];
        snprintf(search_path, sizeof(search_path), "%s/*.input.mmd", GOLDEN_DIR);

        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(search_path, &find_data);

        int count = 0;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                const char* filename = find_data.cFileName;
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext) continue;
                
                size_t name_len = ext - filename;
                if (name_len >= 128) continue; 

                char case_name[128];
                strncpy(case_name, filename, name_len);
                case_name[name_len] = '\0';

                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd"); 

                    REQUIRE(input != NULL);

                    ERDiagram* diagram = er_parse(input);
                    if (!diagram) {
                        // printf("Failed to parse: %s\n", case_name);
                        REQUIRE(diagram != NULL);
                    }

                    char* actual = er_to_json(diagram);
                    REQUIRE(actual != NULL);

                    // Check if we have an expected JSON file
                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");

                    if (expected) {
                        if (!json_equal(actual, expected)) {
                            // printf("JSON mismatch for %s\n", case_name);
                            // printf("Actual: %s\n", actual);
                            // printf("Expected: %s\n", expected);
                            check(0, "JSON mismatch");
                        }
                        free(expected);
                    } else {
                        // If no JSON expectation, at least ensure we parsed end-to-end
                        // printf("No JSON golden for %s, passed parse check.\n", case_name);
                        check(1, "Parse successful");
                    }

                    turbo_json_serialize_free(actual);
                    er_free(diagram);
                    free(input);
                }
                count++;
                if (count >= 1000) break;
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
        // printf("Ran %d ER golden tests.\n", count);
#endif
    }
}
