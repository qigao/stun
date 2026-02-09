#include "tinytest.h"
#include "statediagram/statediagram_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

// Forward declarations
extern StateDiagram* statediagram_parse(const char* input);
extern void statediagram_free(StateDiagram* diagram);
extern char* statediagram_to_json(StateDiagram* diagram);

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

static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (turbo_parse_json((const uint8_t*)a, strlen(a), &ja) != 0) return 0;
    if (turbo_parse_json((const uint8_t*)b, strlen(b), &jb) != 0) {
        turbo_free_json(&ja);
        return 0;
    }

    char *sa = turbo_json_serialize(ja, NULL);
    char *sb = turbo_json_serialize(jb, NULL);
    int eq = (sa && sb && strcmp(sa, sb) == 0);
    turbo_json_serialize_free(sa);
    turbo_json_serialize_free(sb);
    turbo_free_json(&ja);
    turbo_free_json(&jb);
    return eq;
}

spec("statediagram_parser") {
    describe("parsing basics") {
        it("should parse a simple state diagram") {
            const char* input = 
                "stateDiagram-v2\n"
                "  [*] --> Still\n"
                "  Still --> [*]\n"
                "  Still --> Moving\n"
                "  Moving --> Still\n"
                "  Moving --> Crash\n"
                "  Crash --> [*]\n";

            StateDiagram* diagram = statediagram_parse(input);
            REQUIRE(diagram != NULL);
            REQUIRE(diagram->root != NULL);
            
            StateNode* n = diagram->root->nodes;
            REQUIRE(n != NULL);
            
            StateTransition* t = diagram->root->transitions;
            REQUIRE(t != NULL);
            check_str_eq(t->id1, "[*]");
            check_str_eq(t->id2, "Still");
            
            statediagram_free(diagram);
        }

        it("should parse composite states") {
            const char* input = 
                "stateDiagram-v2\n"
                "  state First {\n"
                "    [*] --> Second\n"
                "    Second --> [*]\n"
                "  }\n";

            StateDiagram* diagram = statediagram_parse(input);
            REQUIRE(diagram != NULL);
            
            StateNode* first = diagram->root->nodes;
            REQUIRE(first != NULL);
            check_str_eq(first->id, "First");
            check_int_eq(first->type, STATE_TYPE_COMPOSITE);
            REQUIRE(first->doc != NULL);
            
            StateTransition* t = first->doc->transitions;
            REQUIRE(t != NULL);
            check_str_eq(t->id1, "[*]");
            check_str_eq(t->id2, "Second");
            
            statediagram_free(diagram);
        }
    }

    describe("golden tests") {
#ifdef _WIN32
        char search_path[1024];
        snprintf(search_path, sizeof(search_path), "%s/*.input.mmd", GOLDEN_DIR);

        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(search_path, &find_data);

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

                    StateDiagram* diagram = statediagram_parse(input);
                    if (!diagram) {
                         check(0, "Failed to parse diagram");
                    } else {
                        char* actual = statediagram_to_json(diagram);
                        REQUIRE(actual != NULL);

                        char* expected = load_golden_file(case_name, "output", "json");

                        if (expected) {
                            if (!json_equal(actual, expected)) {
                                printf("JSON mismatch for case: %s\n", case_name);
                                check(0, "JSON mismatch");
                            } else {
                                check(1, "JSON match");
                            }
                            free(expected);
                        } else {
                            check(1, "Parse successful (no golden json)");
                        }

                        turbo_json_serialize_free(actual);
                        statediagram_free(diagram);
                    }
                    if (input) free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#endif
    }
}
