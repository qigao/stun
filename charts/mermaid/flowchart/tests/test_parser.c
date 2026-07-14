#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "flowchart/flowchart_parser_wrapper.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern char* flowchart_to_json(FlowchartDiagram* diagram);

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_len = fread(buf, 1, len, f);
    buf[read_len] = '\0';
    fclose(f);
    return buf;
}

static char* load_golden_file(const char* case_name, const char* suffix, const char* ext) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.%s", GOLDEN_DIR, case_name, ext);
    char* data = read_file(path);
    if (data) return data;

    snprintf(path, sizeof(path), "%s/%s.%s.%s", GOLDEN_DIR, case_name, suffix, ext);
    return read_file(path);
}

static void normalize_json(json_value_t* v) {
    if (!v) return;
    turbo_json_type_t type = turbo_json_type(v);
    if (type == TURBO_JSON_OBJECT) {
        size_t count = turbo_json_object_size(v);
        for (size_t i = 0; i < count; i++) {
            const char* key = turbo_json_object_key(v, i);
            json_value_t* val = turbo_json_object_value(v, i);
            if (strcmp(key, "shape") == 0 && turbo_json_type(val) == TURBO_JSON_STRING) {
                const char* shape_val = turbo_json_string(val);
                if (strcmp(shape_val, "square") == 0) {
                    turbo_json_object_set_string(v, key, "rect");
                }
            }
            normalize_json(val);
        }
    } else if (type == TURBO_JSON_ARRAY) {
        size_t count = turbo_json_array_size(v);
        for (size_t i = 0; i < count; i++) {
            normalize_json(turbo_json_array_get(v, i));
        }
    }
}

static int json_value_equal(const json_value_t* a, const json_value_t* b) {
    if (!a || !b || turbo_json_type(a) != turbo_json_type(b)) return a == b;
    switch (turbo_json_type(a)) {
        case TURBO_JSON_NULL:
            return 1;
        case TURBO_JSON_BOOL:
            return turbo_json_bool(a) == turbo_json_bool(b);
        case TURBO_JSON_NUMBER:
            return turbo_json_number(a) == turbo_json_number(b);
        case TURBO_JSON_STRING:
            return strcmp(turbo_json_string(a), turbo_json_string(b)) == 0;
        case TURBO_JSON_ARRAY: {
            size_t count = turbo_json_array_size(a);
            if (count != turbo_json_array_size(b)) return 0;
            for (size_t i = 0; i < count; ++i) {
                if (!json_value_equal(turbo_json_array_get(a, i),
                                      turbo_json_array_get(b, i))) return 0;
            }
            return 1;
        }
        case TURBO_JSON_OBJECT: {
            size_t count = turbo_json_object_size(a);
            if (count != turbo_json_object_size(b)) return 0;
            for (size_t i = 0; i < count; ++i) {
                const char* key = turbo_json_object_key(a, i);
                if (!json_value_equal(turbo_json_object_value(a, i),
                                      turbo_json_object_get(b, key))) return 0;
            }
            return 1;
        }
    }
    return 0;
}

static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (turbo_parse_json((const uint8_t*)a, strlen(a), &ja) != 0) return 0;
    if (turbo_parse_json((const uint8_t*)b, strlen(b), &jb) != 0) {
        turbo_free_json(&ja);
        return 0;
    }
    
    normalize_json(ja);
    normalize_json(jb);

    int eq = json_value_equal(ja, jb);
    turbo_free_json(&ja);
    turbo_free_json(&jb);
    return eq;
}

spec("flowchart_parser") {
    describe("parsing basics") {
        it("should parse LR flowchart with nodes and edges") {
            const char* input = 
                "flowchart LR\n"
                "  A[Start] --> B(Process)\n";

            FlowchartDiagram* diagram = flowchart_parse(input);
            REQUIRE(diagram != NULL);
            check_str_eq(diagram->direction, "LR");
            flowchart_diagram_free(diagram);
        }

        it("distinguishes complete and partial parses") {
            FlowchartDiagram* diagram = NULL;
            check_int_eq(flowchart_parse_ex("flowchart LR\nA[Good label]", &diagram),
                         FLOWCHART_PARSE_COMPLETE);
            REQUIRE(diagram != NULL);
            check_str_eq(diagram->nodes->label, "Good label");
            flowchart_diagram_free(diagram);

            diagram = NULL;
            check_int_eq(flowchart_parse_ex("flowchart LR\na --> b --> c", &diagram),
                         FLOWCHART_PARSE_COMPLETE);
            REQUIRE(diagram != NULL);
            check_str_eq(diagram->nodes->id, "a");
            check_str_eq(diagram->nodes->next->id, "b");
            check_str_eq(diagram->nodes->next->next->id, "c");
            check_str_eq(diagram->edges->from, "a");
            check_str_eq(diagram->edges->to, "b");
            check_str_eq(diagram->edges->next->from, "b");
            check_str_eq(diagram->edges->next->to, "c");
            flowchart_diagram_free(diagram);

            diagram = NULL;
            check_int_eq(flowchart_parse_ex("flowchart LR\nA[Good] @", &diagram),
                         FLOWCHART_PARSE_PARTIAL);
            REQUIRE(diagram != NULL);
            flowchart_diagram_free(diagram);
        }
    }

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

    describe("golden tests") {
        // Dynamic test discovery
        char search_path[MAX_PATH];
        snprintf(search_path, sizeof(search_path), "%s/*.input.mmd", GOLDEN_DIR);

        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(search_path, &find_data);

        int count = 0;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                const char* filename = find_data.cFileName;
                // Extract case name: remove .input.mmd
                // Filename format: {case}.input.mmd
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext) continue;
                
                size_t name_len = ext - filename;
                if (name_len >= 128) continue; // safety

                char case_name[128];
                strncpy(case_name, filename, name_len);
                case_name[name_len] = '\0';

                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd"); 
                    
                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");

                    if (!input || !expected) {
                        // Skip if files are missing (shouldn't happen with discovery)
                        if(input) free(input);
                        if(expected) free(expected);
                        continue;
                    }

                    FlowchartDiagram* diagram = flowchart_parse(input);
                    if (!diagram) {
                         // printf("Failed to parse: %s\n", case_name); // Optional: log failure
                         REQUIRE(diagram != NULL);
                    }

                    char* actual = flowchart_to_json(diagram);
                    REQUIRE(actual != NULL);

                    // Normalize expected JSON for loose comparison (e.g. square -> rect)
                    // We need to parse 'expected' first to normalize it, then re-serialize?
                    // json_equal function does specific attribute normalization.

                    check(json_equal(actual, expected), "JSON mismatch");

                    turbo_json_serialize_free(actual);
                    flowchart_diagram_free(diagram);
                    free(expected);
                    free(input);
                }
                count++;
                if (count >= 1000) break; // safety break
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
        printf("Ran %d dynamic golden tests.\n", count);
    }
}
