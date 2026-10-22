#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "timeline/timeline_ast.h"
#include "json_parser.h"

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
    if (!(ja = json_parse(a, strlen(a)))) return 0;
    if (!(jb = json_parse(b, strlen(b)))) {
        json_free(ja);
        return 0;
    }
    char *sa = json_serialize(ja, NULL);
    char *sb = json_serialize(jb, NULL);
    int eq = (sa && sb && strcmp(sa, sb) == 0);
    json_serialize_free(sa);
    json_serialize_free(sb);
    json_free(ja);
    json_free(jb);
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
             check_true(diagram->title != NULL && strcmp(diagram->title, "History of World War II") == 0);
             
             REQUIRE(diagram->sections != NULL);
             TimelineSection* s1 = diagram->sections;
             check_true(s1->title != NULL && strcmp(s1->title, "Pre-war") == 0);
             
             REQUIRE(s1->periods != NULL);
             TimelinePeriod* p1 = s1->periods;
             check_true(p1->title != NULL && strcmp(p1->title, "1938") == 0);
             
             REQUIRE(p1->events != NULL);
             TimelineEvent* e1 = p1->events;
             check_true(e1->text != NULL && strcmp(e1->text, "Munich Agreement") == 0);
             check_true(e1->next->text != NULL && strcmp(e1->next->text, "Kristallnacht") == 0);
             
             TimelineSection* s2 = s1->next;
             check_true(s2->title != NULL && strcmp(s2->title, "War Years") == 0);
             
             TimelinePeriod* p2 = s2->periods;
             check_true(p2->title != NULL && strcmp(p2->title, "1939") == 0);
             check_true(p2->next->title != NULL && strcmp(p2->next->title, "1941") == 0);
             
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
             check_true(s->title != NULL && strcmp(s->title, "") == 0); // Default section
             
             REQUIRE(s->periods != NULL);
             check_true(s->periods->title != NULL && strcmp(s->periods->title, "2023") == 0);
             check_true(s->periods->next->title != NULL && strcmp(s->periods->next->title, "2024") == 0);
             
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
             check_true(p1->title != NULL && strcmp(p1->title, "12:00") == 0);
             REQUIRE(p1->events != NULL);
             check_true(p1->events->text != NULL && strcmp(p1->events->text, "Lunch Time") == 0);
             
             TimelinePeriod* p2 = p1->next;
             REQUIRE(p2 != NULL);
             check_true(p2->title != NULL && strcmp(p2->title, "2023-01") == 0);
             REQUIRE(p2->events != NULL);
             check_true(p2->events->text != NULL && strcmp(p2->events->text, "Special : Event") == 0);
             
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
            json_serialize_free(actual);
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
            json_serialize_free(actual);
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
            json_serialize_free(actual);
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
            json_serialize_free(actual);
            timeline_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }
    }
}
