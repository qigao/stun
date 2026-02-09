#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tinytest.h"
#include "xychart/xychart_ast.h"
#include "turbo_parser.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

// Forward declarations
extern XYDiagram* xychart_parse(const char* input);
extern void xychart_free_diagram(XYDiagram* diagram);
extern char* xychart_to_json(XYDiagram* diagram);

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

#define check_double_eq(a, b) do { \
    double diff = (a) - (b); \
    if (diff < 0) diff = -diff; \
    if (diff > 0.0001) { \
        printf("Expected %f, got %f\n", (double)(b), (double)(a)); \
        REQUIRE(0 && "Double mismatch"); \
    } \
} while(0)

spec("xychart_parser") {
    describe("parsing basics") {
        it("should parse simple xychart with categories") {
             const char* input = 
                "xychart-beta\n"
                "title \"Sample Chart\"\n"
                "x-axis \"Categories\" [\"A\", \"B\", \"C\"]\n"
                "y-axis \"Revenue\" 0 --> 100\n"
                "line \"Sales\" [10, 20.5, 30]\n"
                "bar \"Profit\" [5, 12, 18]\n";
             
             XYDiagram* diagram = xychart_parse(input);
             REQUIRE(diagram != NULL);
             check_str_eq(diagram->title, "Sample Chart");
             
             REQUIRE(diagram->orientation == XY_ORIENTATION_VERTICAL);
             
             check_str_eq(diagram->xAxis.title, "Categories");
             REQUIRE(diagram->xAxis.categories.count == 3);
             check_str_eq(diagram->xAxis.categories.items[0], "A");
             check_str_eq(diagram->xAxis.categories.items[1], "B");
             check_str_eq(diagram->xAxis.categories.items[2], "C");
             
             check_str_eq(diagram->yAxis.title, "Revenue");
             REQUIRE(diagram->yAxis.has_range == true);
             check_double_eq(diagram->yAxis.range_min, 0.0);
             check_double_eq(diagram->yAxis.range_max, 100.0);
             
             REQUIRE(diagram->series != NULL);
             XYSeries* s1 = diagram->series;
             REQUIRE(s1->type == XY_SERIES_LINE);
             check_str_eq(s1->name, "Sales");
             REQUIRE(s1->data.count == 3);
             check_double_eq(s1->data.items[0], 10.0);
             check_double_eq(s1->data.items[1], 20.5);
             check_double_eq(s1->data.items[2], 30.0);
             
             XYSeries* s2 = s1->next;
             REQUIRE(s2 != NULL);
             REQUIRE(s2->type == XY_SERIES_BAR);
             check_str_eq(s2->name, "Profit");
             REQUIRE(s2->data.count == 3);
             check_double_eq(s2->data.items[0], 5.0);
             check_double_eq(s2->data.items[1], 12.0);
             check_double_eq(s2->data.items[2], 18.0);
             
             xychart_free_diagram(diagram);
        }

        it("should parse xychart with horizontal orientation") {
             const char* input = 
                "xychart-beta horizontal\n"
                "x-axis 10 --> 50\n";
             
             XYDiagram* diagram = xychart_parse(input);
             REQUIRE(diagram != NULL);
             REQUIRE(diagram->orientation == XY_ORIENTATION_HORIZONTAL);
             REQUIRE(diagram->xAxis.has_range == true);
             check_double_eq(diagram->xAxis.range_min, 10.0);
             check_double_eq(diagram->xAxis.range_max, 50.0);
             
             xychart_free_diagram(diagram);
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

                    XYDiagram* diagram = xychart_parse(input);
                    if (!diagram) {
                         check(0, "Failed to parse diagram");
                    } else {
                        char* actual = xychart_to_json(diagram);
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
                        xychart_free_diagram(diagram);
                    }
                    if (input) free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#endif
    }
}
