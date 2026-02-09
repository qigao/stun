#include "tinytest.h"
#include "journey/journey_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern JourneyDiagram* journey_parse(const char* input);

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

spec("journey_parser") {
    describe("parsing basics") {
        it("should parse simple journey") {
            const char* input = 
                "journey\n"
                "  title My Journey\n"
                "  section Section 1\n"
                "    Task 1: 5: Me\n"
                "    Task 2: 3: Cat\n";

            JourneyDiagram* diagram = journey_parse(input);
            REQUIRE(diagram != NULL);
            
            if(diagram->title) {
                check_str_eq(diagram->title, "My Journey");
            } else {
                // failure
                REQUIRE(0 && "Title is NULL");
            }
            
            JourneySection* s = diagram->sections;
            REQUIRE(s != NULL);
            check_str_eq(s->title, "Section 1");
            
            JourneyTask* t = s->tasks;
            REQUIRE(t != NULL);
            check_str_eq(t->title, "Task 1");
            check_int_eq(t->score, 5);
            REQUIRE(t->actor_count == 1);
            check_str_eq(t->actors[0], "Me");
            
            t = t->next;
            REQUIRE(t != NULL);
            check_str_eq(t->title, "Task 2");
            check_int_eq(t->score, 3);
            check_str_eq(t->actors[0], "Cat");
            
            journey_free_diagram(diagram);
        }

        it("should parse acc title and descr") {
            const char* input = 
                "journey\n"
                "  accTitle: My Title\n"
                "  accDescr: My Description\n";
            
            JourneyDiagram* diagram = journey_parse(input);
            REQUIRE(diagram != NULL);
            check_str_eq(diagram->acc_title, "My Title");
            check_str_eq(diagram->acc_descr, "My Description");
            journey_free_diagram(diagram);
        }
        
        it("should parse multiline acc descr") {
             const char* input = 
                "journey\n"
                "  accDescr { \n"
                "    Line 1\n"
                "    Line 2\n"
                "  }\n";
             
             JourneyDiagram* diagram = journey_parse(input);
             REQUIRE(diagram != NULL);
             REQUIRE(diagram->acc_descr != NULL);
             journey_free_diagram(diagram);
        }
    }

    describe("golden tests") {
        it("basic: my working day") {
            char* mmd_path = golden_path("basic.input.mmd");
            char* json_path = golden_path("basic.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);
            JourneyDiagram* diagram = journey_parse(input);
            REQUIRE(diagram != NULL);
            char* actual = journey_to_json(diagram);
            REQUIRE(actual != NULL);
            check(json_equal(actual, expected), "JSON mismatch");
            turbo_json_serialize_free(actual);
            journey_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }

        it("customer: customer journey") {
            char* mmd_path = golden_path("customer.input.mmd");
            char* json_path = golden_path("customer.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);
            JourneyDiagram* diagram = journey_parse(input);
            REQUIRE(diagram != NULL);
            char* actual = journey_to_json(diagram);
            REQUIRE(actual != NULL);
            check(json_equal(actual, expected), "JSON mismatch");
            turbo_json_serialize_free(actual);
            journey_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }
         it("render-basic: simple journey") {
             char* mmd_path = golden_path("render-basic.mmd");
             char* json_path = golden_path("render-basic.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);

             JourneyDiagram* diagram = journey_parse(input);
             REQUIRE(diagram != NULL);

             char* actual = journey_to_json(diagram);
             REQUIRE(actual != NULL);

             check(json_equal(actual, expected), "JSON mismatch");

             turbo_json_serialize_free(actual);
             journey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }
         it("render-complex: customer support experience") {
             char* mmd_path = golden_path("render-complex.mmd");
             char* json_path = golden_path("render-complex.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);
             JourneyDiagram* diagram = journey_parse(input);
             REQUIRE(diagram != NULL);
             char* actual = journey_to_json(diagram);
             REQUIRE(actual != NULL);
             check(json_equal(actual, expected), "JSON mismatch");
             turbo_json_serialize_free(actual);
             journey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }

         it("comprehensive: all features") {
             char* mmd_path = golden_path("comprehensive.input.mmd");
             char* json_path = golden_path("comprehensive.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);
             JourneyDiagram* diagram = journey_parse(input);
             REQUIRE(diagram != NULL);
             char* actual = journey_to_json(diagram);
             REQUIRE(actual != NULL);
             check(json_equal(actual, expected), "JSON mismatch");
             turbo_json_serialize_free(actual);
             journey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }

         it("edge_cases: weird inputs") {
             char* mmd_path = golden_path("edge_cases.input.mmd");
             char* json_path = golden_path("edge_cases.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);
             JourneyDiagram* diagram = journey_parse(input);
             REQUIRE(diagram != NULL);
             char* actual = journey_to_json(diagram);
             REQUIRE(actual != NULL);
             check(json_equal(actual, expected), "JSON mismatch");
             turbo_json_serialize_free(actual);
             journey_free_diagram(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }
    }
}
