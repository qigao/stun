#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "c4/c4_ast.h"
#include "turbo_parser.h"
#include "../src/c4_parser_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

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

spec("C4 Parser") {
    describe("Context Diagram") {
        it("should parse basic person and system") {
            const char* input =
                "C4Context\n"
                "Person(customerA, \"Banking Customer A\", \"A customer\")\n"
                "System(systemA, \"Banking System\", \"Allows customers to view accounts\")\n"
                "Rel(customerA, systemA, \"Uses\")\n";

            C4Diagram* diagram = c4_parse(input);
            check_ptr_ne(diagram, NULL);
            check_int_eq(diagram->type, C4_DT_CONTEXT);

            // Check elements
            C4Element* el = diagram->elements;
            check_ptr_ne(el, NULL);
            check_str_eq(el->alias, "customerA");
            check_int_eq(el->type, C4_EL_PERSON);
            check_str_eq(el->label, "Banking Customer A");

            el = el->next;
            check_ptr_ne(el, NULL);
            check_str_eq(el->alias, "systemA");
            check_int_eq(el->type, C4_EL_SYSTEM);

            // Check relationships
            C4Rel* rel = diagram->relationships;
            check_ptr_ne(rel, NULL);
            check_str_eq(rel->from, "customerA");
            check_str_eq(rel->to, "systemA");
            check_str_eq(rel->label, "Uses");

            c4_free_diagram(diagram);
        }
    }

    describe("Container Diagram") {
        it("should parse container elements") {
             const char* input =
                "C4Container\n"
                "Container(web_app, \"Web Application\", \"Java, Spring MVC\", \"Delivers content\")\n";

            C4Diagram* diagram = c4_parse(input);
            check_ptr_ne(diagram, NULL);
            check_int_eq(diagram->type, C4_DT_CONTAINER);

            C4Element* el = diagram->elements;
            check_ptr_ne(el, NULL);
            check_str_eq(el->alias, "web_app");
            check_int_eq(el->type, C4_EL_CONTAINER);
            check_str_eq(el->technology, "Java, Spring MVC");

            c4_free_diagram(diagram);
        }
    }

    describe("Boundaries") {
        it("should parse system boundary") {
            const char* input =
                "C4Context\n"
                "System_Boundary(b1, \"Bank Boundary\") {\n"
                "  System(backend, \"Backend\")\n"
                "}\n";

            C4Diagram* diagram = c4_parse(input);
            check_ptr_ne(diagram, NULL);

            // First element is the boundary
            C4Element* boundary = diagram->elements;
            check_ptr_ne(boundary, NULL);
            check_int_eq(boundary->type, C4_EL_SYSTEM_BOUNDARY);

            // Child is in children list, not next (flat)
            C4Element* child = boundary->children;
            check_ptr_ne(child, NULL);
            check_str_eq(child->alias, "backend");
            check_ptr_eq(child->parent, boundary);
            check_ptr_eq(child->next, NULL); // No siblings for child

            c4_free_diagram(diagram);
        }

        it("should parse nested boundaries") {
            const char* input =
                "C4Context\n"
                "Enterprise_Boundary(ent, \"Enterprise\") {\n"
                "  System_Boundary(sys, \"System\") {\n"
                "    System(app, \"App\")\n"
                "  }\n"
                "}\n";

            C4Diagram* diagram = c4_parse(input);
            check_ptr_ne(diagram, NULL);

            C4Element* ent = diagram->elements;
            check_ptr_ne(ent, NULL);
            check_int_eq(ent->type, C4_EL_ENTERPRISE_BOUNDARY);

            C4Element* sys = ent->children;
            check_ptr_ne(sys, NULL);
            check_int_eq(sys->type, C4_EL_SYSTEM_BOUNDARY);
            check_ptr_eq(sys->parent, ent);

            C4Element* app = sys->children;
            check_ptr_ne(app, NULL);
            check_int_eq(app->type, C4_EL_SYSTEM);
            check_ptr_eq(app->parent, sys);

            c4_free_diagram(diagram);
        }
    }

    describe("Attributes") {
        it("should parse mixed attributes") {
             const char* input =
                "C4Context\n"
                "Person(alias, \"Label with spaces\", unquoted_tech)\n";

            C4Diagram* diagram = c4_parse(input);
            check_ptr_ne(diagram, NULL);

            C4Element* el = diagram->elements;
            check_str_eq(el->alias, "alias");
            check_str_eq(el->label, "Label with spaces");
            check_str_eq(el->descr, "unquoted_tech");

            c4_free_diagram(diagram);
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

                    C4Diagram* diagram = c4_parse(input);
                    if (!diagram) {
                        REQUIRE(diagram != NULL);
                    }

                    char* actual = c4_to_json(diagram);
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
                    c4_free_diagram(diagram);
                    free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#endif
    }
}
