#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "gantt/gantt_ast.h"
#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern GanttDiagram* gantt_parse(const char* input);
extern void gantt_diagram_free(GanttDiagram* diagram);
extern char* gantt_to_json(GanttDiagram* diagram);

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(size + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);
    return data;
}

static char* golden_path(const char* name, const char* ext) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/%s.%s", GOLDEN_DIR, name, ext);
    return path;
}

// Compare two JSON strings by parsing and re-serializing to compact form
static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (json_parse((const uint8_t*)a, strlen(a), &ja) != 0) return 0;
    if (json_parse((const uint8_t*)b, strlen(b), &jb) != 0) {
        json_free(&ja);
        return 0;
    }
    char *sa = json_serialize(ja, NULL);
    char *sb = json_serialize(jb, NULL);
    int eq = (sa && sb && strcmp(sa, sb) == 0);
    json_serialize_free(sa);
    json_serialize_free(sb);
    json_free(&ja);
    json_free(&jb);
    return eq;
}

spec("gantt_parser") {
    describe("parsing basics") {
        it("should parse a simple gantt chart") {
            const char* input = 
                "gantt\n"
                "  title A simple gantt chart\n"
                "  dateFormat YYYY-MM-DD\n"
                "  section Section 1\n"
                "  A task :a1, 2023-01-01, 30d\n";

            GanttDiagram* diagram = gantt_parse(input);
            REQUIRE(diagram != NULL);
            check_str_eq(diagram->title, "A simple gantt chart");
            check_str_eq(diagram->date_format, "YYYY-MM-DD");
            
            REQUIRE(diagram->sections != NULL);
            check_str_eq(diagram->sections->name, "Section 1");
            
            REQUIRE(diagram->sections->tasks != NULL);
            check_str_eq(diagram->sections->tasks->name, "A task");
            check_str_eq(diagram->sections->tasks->id, "a1");
            check_str_eq(diagram->sections->tasks->start, "2023-01-01");
            check_str_eq(diagram->sections->tasks->end, "30d");
            
            gantt_diagram_free(diagram);
        }

        it("should parse tasks with status") {
            const char* input = 
                "gantt\n"
                "  section Done tasks\n"
                "  Completed :done, 2023-01-01, 10d\n"
                "  Active :active, 2023-01-11, 5d\n";

            GanttDiagram* diagram = gantt_parse(input);
            REQUIRE(diagram != NULL);
            
            GanttSection* s = diagram->sections;
            REQUIRE(s != NULL);
            
            GanttTask* t = s->tasks;
            REQUIRE(t != NULL);
            check_str_eq(t->name, "Completed");
            check_int_eq(t->status, GANTT_STATUS_DONE);
            
            t = t->next;
            REQUIRE(t != NULL);
            check_str_eq(t->name, "Active");
            check_int_eq(t->status, GANTT_STATUS_ACTIVE);
            
            gantt_diagram_free(diagram);
        }
    }

    describe("golden tests") {
        const char* cases[] = {
            "render-basic",
            "render-complex"
        };

        for (int i = 0; i < 2; i++) {
            it(cases[i]) {
                char* mmd = read_file(golden_path(cases[i], "mmd"));
                char* expected = read_file(golden_path(cases[i], "json"));
                REQUIRE(mmd != NULL);
                REQUIRE(expected != NULL);

                GanttDiagram* diagram = gantt_parse(mmd);
                REQUIRE(diagram != NULL);

                char* actual = gantt_to_json(diagram);
                REQUIRE(actual != NULL);
                
                if (!json_equal(actual, expected)) {
                    printf("\n=== ACTUAL JSON [%s] ===\n%s\n", cases[i], actual);
                    printf("=== EXPECTED JSON [%s] ===\n%s\n", cases[i], expected);
                }

                check(json_equal(actual, expected), "JSON mismatch");

                json_serialize_free(actual);
                gantt_diagram_free(diagram);
                free(mmd);
                free(expected);
            }
        }
    }
}
