#include "tinytest.h"
#include "block/block_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef REQUIRE
#define REQUIRE(cond) do { if (!(cond)) { check(0, #cond); return; } } while (0)
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

extern BlockDiagram* block_parse(const char* input);
extern void block_free(BlockDiagram* diagram);
extern char* block_to_json(BlockDiagram* diagram);

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

spec("block_parser") {
    describe("parsing basics") {
        it("should sanity check manual input") {
            const char* input = "block-beta\n  columns 2\n  A\n  B\n";
            BlockDiagram* d = block_parse(input);
            REQUIRE(d != NULL);
            block_free(d);
        }
    }

    describe("golden tests") {
#ifdef _WIN32
        char search_path[MAX_PATH];
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

                    BlockDiagram* diagram = block_parse(input);
                    if (!diagram) {
                        REQUIRE(diagram != NULL);
                    }

                    char* actual = block_to_json(diagram);
                    REQUIRE(actual != NULL);

                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");

                    if (expected) {
                        if (!json_equal(actual, expected)) {
                            check(0, "JSON mismatch");
                        }
                        free(expected);
                    } else {
                        check(1, "Parse successful");
                    }

                    turbo_json_serialize_free(actual);
                    block_free(diagram);
                    free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#endif
    }
}
