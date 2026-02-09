#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tinytest.h"
#include "timeline/timeline_ast.h"
#include "turbo_parser.h"

extern TimelineDiagram* timeline_parse(const char* input);
extern void timeline_free_diagram(TimelineDiagram* diagram);

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

spec("timeline_parser") {
    describe("parsing basics") {
        it("should parse simple timeline chart") {
             const char* input = 
                "timeline\n"
                "    title History of World War II\n"
                "    section Pre-war\n"
                "        1938 : Munich Agreement\n"
                "             : Kristallnacht\n"
                "    section War Years\n"
                "        1939 : Invasion of Poland\n"
                "        1941 : Pearl Harbor\n";
             
             TimelineDiagram* diagram = timeline_parse(input);
             REQUIRE(diagram != NULL);
             check_str_eq(diagram->title, "History of World War II");
             
             REQUIRE(diagram->sections != NULL);
             TimelineSection* s1 = diagram->sections;
             check_str_eq(s1->title, "Pre-war");
             
             REQUIRE(s1->periods != NULL);
             TimelinePeriod* p1 = s1->periods;
             check_str_eq(p1->title, "1938");
             
             REQUIRE(p1->events != NULL);
             TimelineEvent* e1 = p1->events;
             check_str_eq(e1->text, "Munich Agreement");
             check_str_eq(e1->next->text, "Kristallnacht");
             
             TimelineSection* s2 = s1->next;
             check_str_eq(s2->title, "War Years");
             
             TimelinePeriod* p2 = s2->periods;
             check_str_eq(p2->title, "1939");
             check_str_eq(p2->next->title, "1941");
             
             timeline_free_diagram(diagram);
        }

        it("should parse timeline without sections") {
             const char* input = 
                "timeline\n"
                "    2023 : Event A\n"
                "    2024 : Event B\n";
             
             TimelineDiagram* diagram = timeline_parse(input);
             REQUIRE(diagram != NULL);
             
             REQUIRE(diagram->sections != NULL);
             TimelineSection* s = diagram->sections;
             check_str_eq(s->title, ""); // Default section
             
             REQUIRE(s->periods != NULL);
             check_str_eq(s->periods->title, "2023");
             check_str_eq(s->periods->next->title, "2024");
             
             timeline_free_diagram(diagram);
        }

        it("should handle periods with colons and avoid greedy matching") {
             const char* input = 
                "timeline\n\n\n" // Testing multiple newlines
                "    12:00 : Lunch Time\n"
                "    2023-01 : Special : Event\n";
             
             TimelineDiagram* diagram = timeline_parse(input);
             REQUIRE(diagram != NULL);
             
             TimelineSection* s = diagram->sections;
             REQUIRE(s != NULL);
             
             TimelinePeriod* p1 = s->periods;
             REQUIRE(p1 != NULL);
             check_str_eq(p1->title, "12:00");
             REQUIRE(p1->events != NULL);
             check_str_eq(p1->events->text, "Lunch Time");
             
             TimelinePeriod* p2 = p1->next;
             REQUIRE(p2 != NULL);
             check_str_eq(p2->title, "2023-01");
             REQUIRE(p2->events != NULL);
             check_str_eq(p2->events->text, "Special : Event");
             
             timeline_free_diagram(diagram);
        }
    }

    describe("golden tests") {
        it("basic: history of social media") {
            char* mmd_path = golden_path("basic.input.mmd");
            char* json_path = golden_path("basic.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);
            TimelineDiagram* diagram = timeline_parse(input);
            REQUIRE(diagram != NULL);
            char* actual = timeline_to_json(diagram);
            REQUIRE(actual != NULL);
            check(json_equal(actual, expected), "JSON mismatch");
            turbo_json_serialize_free(actual);
            timeline_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }

        it("sections: project milestones") {
            char* mmd_path = golden_path("sections.input.mmd");
            char* json_path = golden_path("sections.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);
            TimelineDiagram* diagram = timeline_parse(input);
            REQUIRE(diagram != NULL);
            char* actual = timeline_to_json(diagram);
            REQUIRE(actual != NULL);
            check(json_equal(actual, expected), "JSON mismatch");
            turbo_json_serialize_free(actual);
            timeline_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }
        it("render-basic: single section, single period, single event") {
            char* mmd_path = golden_path("render-basic.mmd");
            char* json_path = golden_path("render-basic.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);
            TimelineDiagram* diagram = timeline_parse(input);
            REQUIRE(diagram != NULL);
            char* actual = timeline_to_json(diagram);
            REQUIRE(actual != NULL);
            check(json_equal(actual, expected), "JSON mismatch");
            turbo_json_serialize_free(actual);
            timeline_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }

        it("render-complex: multi section, multi period, multi event") {
            char* mmd_path = golden_path("render-complex.mmd");
            char* json_path = golden_path("render-complex.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);
            TimelineDiagram* diagram = timeline_parse(input);
            REQUIRE(diagram != NULL);
            char* actual = timeline_to_json(diagram);
            REQUIRE(actual != NULL);
            check(json_equal(actual, expected), "JSON mismatch");
            turbo_json_serialize_free(actual);
            timeline_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }
    }
}
