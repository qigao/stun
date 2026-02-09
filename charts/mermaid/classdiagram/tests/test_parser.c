#include "tinytest.h"
#include "classdiagram/classdiagram_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

extern ClassDiagram* classdiagram_parse(const char* input);
extern void classdiagram_free(ClassDiagram* diagram);
extern char* classdiagram_to_json(ClassDiagram* diagram);

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

spec("classdiagram_parser") {
    describe("parsing basics") {
        it("should parse a simple class diagram") {
            const char* input =
                "classDiagram\n"
                "  class Animal\n"
                "  Animal : +name String\n"
                "  Animal : +isAlive() Boolean\n";

            ClassDiagram* diagram = classdiagram_parse(input);
            REQUIRE(diagram != NULL);

            REQUIRE(diagram->classes != NULL);
            check_str_eq(diagram->classes->name, "Animal");

            ClassMember* m = diagram->classes->members;
            REQUIRE(m != NULL);
            check_str_eq(m->name, "name");
            check_str_eq(m->type, "String");
            check_int_eq(m->visibility, CLASS_VISIBILITY_PUBLIC);
            check_int_eq(m->is_method, 0);

            m = m->next;
            REQUIRE(m != NULL);
            check_str_eq(m->name, "isAlive");
            check_str_eq(m->return_type, "Boolean");
            check_int_eq(m->visibility, CLASS_VISIBILITY_PUBLIC);
            check_int_eq(m->is_method, 1);

            classdiagram_free(diagram);
        }

        it("should parse relationships") {
            const char* input =
                "classDiagram\n"
                "  Animal --|> Duck\n"
                "  Duck \"1\" --* \"*\" Egg\n";

            ClassDiagram* diagram = classdiagram_parse(input);
            REQUIRE(diagram != NULL);

            ClassRelationship* r = diagram->relationships;
            REQUIRE(r != NULL);

            check_str_eq(r->from, "Animal");
            check_str_eq(r->to, "Duck");

            r = r->next;
            REQUIRE(r != NULL);
            check_str_eq(r->from, "Duck");
            check_str_eq(r->to, "Egg");
            check_str_eq(r->from_cardinality, "1");
            check_str_eq(r->to_cardinality, "*");

            classdiagram_free(diagram);
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

                    ClassDiagram* diagram = classdiagram_parse(input);
                    if (!diagram) {
                        REQUIRE(diagram != NULL);
                    }

                    char* actual = classdiagram_to_json(diagram);
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
                    classdiagram_free(diagram);
                    free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#endif
    }
}
