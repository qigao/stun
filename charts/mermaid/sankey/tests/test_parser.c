#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tinytest.h"
#include "sankey/sankey_ast.h"
#include "turbo_parser.h"

#ifndef REQUIRE
#define REQUIRE(cond) do { if (!(cond)) { check(0, #cond); return; } } while (0)
#endif

extern SankeyDiagram* sankey_parse(const char* input);
extern void sankey_free_diagram(SankeyDiagram* diagram);

#ifndef GOLDEN_DIR
#define GOLDEN_DIR "."
#endif

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static char* golden_path(const char* filename) {
    size_t dlen = strlen(GOLDEN_DIR);
    size_t flen = strlen(filename);
    char* path = (char*)malloc(dlen + 1 + flen + 1);
    sprintf(path, "%s/%s", GOLDEN_DIR, filename);
    return path;
}

// Compare two JSON strings by parsing and re-serializing to compact form
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

#ifdef check_double_eq
#undef check_double_eq
#endif
#define check_double_eq(a, b) do { \
    double diff = (a) - (b); \
    if (diff < 0) diff = -diff; \
    if (diff > 0.0001) { \
        printf("Expected %f, got %f\n", (double)(b), (double)(a)); \
        REQUIRE(0 && "Double mismatch"); \
    } \
} while(0)

spec("sankey_parser") {
    describe("parsing basics") {
        it("should parse simple sankey chart") {
             const char* input = 
                "sankey-beta\n"
                "source,target,10\n"
                "a,b,5\n";
             
             SankeyDiagram* diagram = sankey_parse(input);
             REQUIRE(diagram != NULL);
             
             REQUIRE(diagram->links != NULL);
             SankeyLink* l1 = diagram->links;
             check_str_eq(l1->source, "source");
             check_str_eq(l1->target, "target");
             check_double_eq(l1->value, 10.0);
             
             SankeyLink* l2 = l1->next;
             REQUIRE(l2 != NULL);
             check_str_eq(l2->source, "a");
             check_str_eq(l2->target, "b");
             check_double_eq(l2->value, 5.0);
             
             sankey_free_diagram(diagram);
        }

        it("should parse quoted fields") {
             const char* input = 
                "sankey-beta\n"
                "\"Long Source Name\",\"Target, Name\",12.5\n";
             
             SankeyDiagram* diagram = sankey_parse(input);
             REQUIRE(diagram != NULL);
             
             REQUIRE(diagram->links != NULL);
             SankeyLink* l = diagram->links;
             check_str_eq(l->source, "Long Source Name");
             check_str_eq(l->target, "Target, Name");
             check_double_eq(l->value, 12.5);
             
             sankey_free_diagram(diagram);
        }

        it("should evaluate link value expressions with MIR") {
             const char* input =
                "sankey-beta\n"
                "source,target,2*3\n"
                "a,b,\"pow(2,3)\"\n";

             SankeyDiagram* diagram = sankey_parse(input);
             if (!diagram) {
                 check(0, "diagram parsed");
                 return;
             }

             if (!diagram->links) {
                 check(0, "first link parsed");
                 sankey_free_diagram(diagram);
                 return;
             }
             check_double_eq(diagram->links->value, 6.0);

             if (!diagram->links->next) {
                 check(0, "second link parsed");
                 sankey_free_diagram(diagram);
                 return;
             }
             check_double_eq(diagram->links->next->value, 8.0);

             sankey_free_diagram(diagram);
        }
    }

    describe("golden tests") {
         it("render-simple") {
             char* mmd_path = golden_path("render-simple.mmd");
             char* json_path = golden_path("render-simple.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);

             SankeyDiagram* diagram = sankey_parse(input);
             REQUIRE(diagram != NULL);

             char* actual = sankey_to_json(diagram);
             REQUIRE(actual != NULL);

             check(json_equal(actual, expected), "JSON mismatch");

             turbo_json_serialize_free(actual);
             sankey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }

         it("render-complex") {
             char* mmd_path = golden_path("render-complex.mmd");
             char* json_path = golden_path("render-complex.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);

             SankeyDiagram* diagram = sankey_parse(input);
             REQUIRE(diagram != NULL);

             char* actual = sankey_to_json(diagram);
             REQUIRE(actual != NULL);

             check(json_equal(actual, expected), "JSON mismatch");

             turbo_json_serialize_free(actual);
             sankey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }

         it("render-decimal") {
             char* mmd_path = golden_path("render-decimal.mmd");
             char* json_path = golden_path("render-decimal.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);

             SankeyDiagram* diagram = sankey_parse(input);
             REQUIRE(diagram != NULL);

             char* actual = sankey_to_json(diagram);
             REQUIRE(actual != NULL);

             check(json_equal(actual, expected), "JSON mismatch");

             turbo_json_serialize_free(actual);
             sankey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }
    }
}
