#include "tinytest.h"
#include "quadrant/quadrant_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

extern QuadrantDiagram* quadrant_parse(const char* input);
extern void quadrant_free_diagram(QuadrantDiagram* diagram);
extern char* quadrant_to_json(QuadrantDiagram* diagram);

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

spec("quadrant_parser") {
    describe("parsing basics") {
        it("should parse simple quadrant chart") {
             const char* input = 
                "quadrantChart\n"
                "  title Reach and engagement of campaigns\n"
                "  x-axis Low Reach --> High Reach\n"
                "  y-axis Low Engagement --> High Engagement\n"
                "  quadrant-1 We should expand\n"
                "  quadrant-2 Need to promote\n"
                "  quadrant-3 Re-evaluate\n"
                "  quadrant-4 May be improved\n"
                "  Campaign A: [0.3, 0.6]\n"
                "  Campaign B: [0.45, 0.23]\n";
             
             QuadrantDiagram* diagram = quadrant_parse(input);
             REQUIRE(diagram != NULL);
             check_str_eq(diagram->title, "Reach and engagement of campaigns");
             check_str_eq(diagram->xAxisLeft, "Low Reach");
             check_str_eq(diagram->xAxisRight, "High Reach");
             check_str_eq(diagram->yAxisBottom, "Low Engagement");
             check_str_eq(diagram->yAxisTop, "High Engagement");
             
             check_str_eq(diagram->quadrant1Text, "We should expand");
             check_str_eq(diagram->quadrant2Text, "Need to promote");
             check_str_eq(diagram->quadrant3Text, "Re-evaluate");
             check_str_eq(diagram->quadrant4Text, "May be improved");
             
             REQUIRE(diagram->points != NULL);
             QuadrantPoint* p1 = diagram->points;
             check_str_eq(p1->text, "Campaign A");
             check_double_eq(p1->x, 0.3);
             check_double_eq(p1->y, 0.6);
             
             QuadrantPoint* p2 = p1->next;
             REQUIRE(p2 != NULL);
             check_str_eq(p2->text, "Campaign B");
             check_double_eq(p2->x, 0.45);
             check_double_eq(p2->y, 0.23);
             
             quadrant_free_diagram(diagram);
        }

        it("should parse points with classes") {
             const char* input = 
                "quadrantChart\n"
                "  Campaign C :::urgent [0.1, 0.1]\n";
             
             QuadrantDiagram* diagram = quadrant_parse(input);
             REQUIRE(diagram != NULL);
             
             REQUIRE(diagram->points != NULL);
             QuadrantPoint* p = diagram->points;
             check_str_eq(p->text, "Campaign C");
             check_str_eq(p->className, "urgent");
             check_double_eq(p->x, 0.1);
             check_double_eq(p->y, 0.1);
             
             quadrant_free_diagram(diagram);
        }
    }

    describe("golden tests") {
#if defined(_WIN32) && !defined(__MINGW32__)
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

                    QuadrantDiagram* diagram = quadrant_parse(input);
                    if (!diagram) {
                        REQUIRE(diagram != NULL);
                    }

                    char* actual = quadrant_to_json(diagram);
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
                    quadrant_free_diagram(diagram);
                    free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR* dir = opendir(GOLDEN_DIR);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                const char* filename = entry->d_name;
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

                    QuadrantDiagram* diagram = quadrant_parse(input);
                    if (!diagram) {
                        REQUIRE(diagram != NULL);
                    }

                    char* actual = quadrant_to_json(diagram);
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
                    quadrant_free_diagram(diagram);
                    free(input);
                }
            }
            closedir(dir);
        }
#endif
    }
}
