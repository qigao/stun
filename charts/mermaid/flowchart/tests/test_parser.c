#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "flowchart/flowchart_parser_wrapper.h"
#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

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
    json_type_t type = json_type(v);
    if (type == JSON_OBJECT) {
        size_t count = json_object_size(v);
        for (size_t i = 0; i < count; i++) {
            const char* key = json_object_key(v, i);
            json_value_t* val = json_object_value(v, i);
            if (strcmp(key, "shape") == 0 && json_type(val) == JSON_STRING) {
                const char* shape_val = json_string(val);
                if (strcmp(shape_val, "square") == 0) {
                    json_object_set_string(v, key, "rect");
                }
            }
            normalize_json(val);
        }
    } else if (type == JSON_ARRAY) {
        size_t count = json_array_size(v);
        for (size_t i = 0; i < count; i++) {
            normalize_json(json_array_get(v, i));
        }
    }
}

static int json_value_equal(const json_value_t* a, const json_value_t* b) {
    if (!a || !b || json_type(a) != json_type(b)) return a == b;
    switch (json_type(a)) {
        case JSON_NULL:
            return 1;
        case JSON_BOOL:
            return json_bool(a) == json_bool(b);
        case JSON_NUMBER:
            return json_number(a) == json_number(b);
        case JSON_STRING:
            return strcmp(json_string(a), json_string(b)) == 0;
        case JSON_ARRAY: {
            size_t count = json_array_size(a);
            if (count != json_array_size(b)) return 0;
            for (size_t i = 0; i < count; ++i) {
                if (!json_value_equal(json_array_get(a, i),
                                      json_array_get(b, i))) return 0;
            }
            return 1;
        }
        case JSON_OBJECT: {
            size_t count = json_object_size(a);
            if (count != json_object_size(b)) return 0;
            for (size_t i = 0; i < count; ++i) {
                const char* key = json_object_key(a, i);
                if (!json_value_equal(json_object_value(a, i),
                                      json_object_get(b, key))) return 0;
            }
            return 1;
        }
    }
    return 0;
}

static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (!(ja = json_parse(a, strlen(a)))) return 0;
    if (!(jb = json_parse(b, strlen(b)))) {
        json_free(ja);
        return 0;
    }
    
    normalize_json(ja);
    normalize_json(jb);

    int eq = json_value_equal(ja, jb);
    json_free(ja);
    json_free(jb);
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
            check_true(strcmp(diagram->direction, "LR") == 0);
            flowchart_diagram_free(diagram);
        }

        it("distinguishes complete and partial parses") {
            FlowchartDiagram* diagram = NULL;
            check_true(flowchart_parse_ex("flowchart LR\nA[Good label]", &diagram) ==
                       FLOWCHART_PARSE_COMPLETE);
            REQUIRE(diagram != NULL);
            check_true(strcmp(diagram->nodes->label, "Good label") == 0);
            flowchart_diagram_free(diagram);

            diagram = NULL;
            check_true(flowchart_parse_ex("flowchart LR\na --> b --> c", &diagram) ==
                       FLOWCHART_PARSE_COMPLETE);
            REQUIRE(diagram != NULL);
            check_true(strcmp(diagram->nodes->id, "a") == 0);
            check_true(strcmp(diagram->nodes->next->id, "b") == 0);
            check_true(strcmp(diagram->nodes->next->next->id, "c") == 0);
            check_true(strcmp(diagram->edges->from, "a") == 0);
            check_true(strcmp(diagram->edges->to, "b") == 0);
            check_true(strcmp(diagram->edges->next->from, "b") == 0);
            check_true(strcmp(diagram->edges->next->to, "c") == 0);
            flowchart_diagram_free(diagram);

            diagram = NULL;
            check_true(flowchart_parse_ex("flowchart LR\nA[Good] @", &diagram) ==
                       FLOWCHART_PARSE_PARTIAL);
            REQUIRE(diagram != NULL);
            flowchart_diagram_free(diagram);
        }
    }

    describe("golden tests") {
        int count = 0;
        int golden_failures = 0;
#ifdef _WIN32
        char search_path[512];
        snprintf(search_path, sizeof(search_path), "%s/*.input.mmd", GOLDEN_DIR);
        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(search_path, &find_data);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                const char* filename = find_data.cFileName;
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext) continue;
                size_t name_len = (size_t)(ext - filename);
                if (name_len >= 128) continue;
                char case_name[128];
                memcpy(case_name, filename, name_len);
                case_name[name_len] = '\0';
                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd");
                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");
                    int failed = !input || !expected;
                    FlowchartDiagram* diagram = NULL;
                    char* actual = NULL;
                    if (!failed) {
                        diagram = flowchart_parse(input);
                        if (!diagram) {
                            failed = 1;
                        } else {
                            actual = flowchart_to_json(diagram);
                            if (!actual || !json_equal(actual, expected))
                                failed = 1;
                        }
                    }
                    if (actual) json_serialize_free(actual);
                    if (diagram) flowchart_diagram_free(diagram);
                    free(expected);
                    free(input);
                    /* Defer assertions until the directory handle is closed. */
                    if (failed) {
                        fprintf(stderr, "Flowchart golden mismatch: %s\n", case_name);
                        if (golden_failures < 3) {
                            fprintf(stderr, "--- expected JSON (%s) ---\n%s\n",
                                    case_name, expected ? expected : "<missing>");
                            fprintf(stderr, "--- actual JSON (%s) ---\n%s\n",
                                    case_name, actual ? actual : "<parse/serialize failed>");
                        }
                        golden_failures++;
                    }
                }
                if (++count >= 1000) break;
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR* directory = opendir(GOLDEN_DIR);
        check_true(directory != NULL);
        if (directory) {
            struct dirent* entry;
            while ((entry = readdir(directory)) != NULL) {
                const char* filename = entry->d_name;
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext || ext[strlen(".input.mmd")] != '\0') continue;
                size_t name_len = (size_t)(ext - filename);
                if (name_len >= 128) continue;
                char case_name[128];
                memcpy(case_name, filename, name_len);
                case_name[name_len] = '\0';
                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd");
                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");
                    int failed = !input || !expected;
                    FlowchartDiagram* diagram = NULL;
                    char* actual = NULL;
                    if (!failed) {
                        diagram = flowchart_parse(input);
                        if (!diagram) {
                            failed = 1;
                        } else {
                            actual = flowchart_to_json(diagram);
                            if (!actual || !json_equal(actual, expected))
                                failed = 1;
                        }
                    }
                    if (actual) json_serialize_free(actual);
                    if (diagram) flowchart_diagram_free(diagram);
                    free(expected);
                    free(input);
                    /* Defer assertions until the directory handle is closed. */
                    if (failed) {
                        fprintf(stderr, "Flowchart golden mismatch: %s\n", case_name);
                        if (golden_failures < 3) {
                            fprintf(stderr, "--- expected JSON (%s) ---\n%s\n",
                                    case_name, expected ? expected : "<missing>");
                            fprintf(stderr, "--- actual JSON (%s) ---\n%s\n",
                                    case_name, actual ? actual : "<parse/serialize failed>");
                        }
                        golden_failures++;
                    }
                }
                if (++count >= 1000) break;
            }
            closedir(directory);
        }
#endif
        check_true(golden_failures == 0);
        printf("Ran %d dynamic golden tests.\n", count);
    }
}