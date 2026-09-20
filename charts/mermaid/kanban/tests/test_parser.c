#include "tinytest.h"
#include "kanban/kanban_ast.h"
#include <json_parser.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef REQUIRE
#define REQUIRE(cond) do { if (!(cond)) { check(0, #cond); return; } } while (0)
#endif

extern KanbanDiagram* kanban_parse(const char* input);
extern void kanban_free_diagram(KanbanDiagram* diagram);
extern char* kanban_to_json(KanbanDiagram* diagram);

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
    if ((ja = json_parse(a, strlen(a))) == NULL) return 0;
    if ((jb = json_parse(b, strlen(b))) == NULL) {
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

spec("kanban_parser") {
    describe("parsing basics") {
        it("should parse simple structure") {
             const char* input = 
                "kanban\n"
                "  Todo\n"
                "    [Task 1]\n";
             
             KanbanDiagram* diagram = kanban_parse(input);
             REQUIRE(diagram != NULL);
             REQUIRE(diagram->root != NULL);
             // Root is dummy. Children should be Todo.
             REQUIRE(diagram->root->children != NULL);
             KanbanNode* todo = diagram->root->children;
             check_str_eq(todo->label, "Todo");
             
             REQUIRE(todo->children != NULL);
             KanbanNode* task1 = todo->children;
             check_str_eq(task1->label, "Task 1");
             REQUIRE(task1->type == KANBAN_NODE_SQUARE);
             
             kanban_free_diagram(diagram);
        }

        it("should parse complex hierarchy") {
             const char* input = 
                "kanban\n"
                "  Col1\n"
                "    Item1\n"
                "  Col2\n"
                "    Item2\n";
             
             KanbanDiagram* diagram = kanban_parse(input);
             REQUIRE(diagram != NULL);
             
             KanbanNode* col1 = diagram->root->children;
             REQUIRE(col1 != NULL);
             check_str_eq(col1->label, "Col1");
             
             KanbanNode* col2 = col1->next;
             REQUIRE(col2 != NULL);
             check_str_eq(col2->label, "Col2");
             
             REQUIRE(col1->children != NULL);
             check_str_eq(col1->children->label, "Item1");
             
             REQUIRE(col2->children != NULL);
             check_str_eq(col2->children->label, "Item2");
             
             kanban_free_diagram(diagram);
        }
        
        it("should parse flat list") {
             const char* input = 
                "kanban\n"
                "Item1\n"
                "Item2\n";
             
             KanbanDiagram* diagram = kanban_parse(input);
             REQUIRE(diagram != NULL);
             
             KanbanNode* item1 = diagram->root->children;
             REQUIRE(item1 != NULL);
             check_str_eq(item1->label, "Item1");
             
             KanbanNode* item2 = item1->next;
             REQUIRE(item2 != NULL);
             check_str_eq(item2->label, "Item2");
             
             kanban_free_diagram(diagram);
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

            KanbanDiagram* diagram = kanban_parse(input);
            REQUIRE(diagram != NULL);

            char* actual = kanban_to_json(diagram);
            REQUIRE(actual != NULL);

            check(json_equal(actual, expected), "JSON mismatch");

            json_serialize_free(actual);
            kanban_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }

        it("render-shapes") {
            char* mmd_path = golden_path("render-shapes.mmd");
            char* json_path = golden_path("render-shapes.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);

            KanbanDiagram* diagram = kanban_parse(input);
            REQUIRE(diagram != NULL);

            char* actual = kanban_to_json(diagram);
            REQUIRE(actual != NULL);

            check(json_equal(actual, expected), "JSON mismatch");

            json_serialize_free(actual);
            kanban_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }

        it("render-decorations") {
            char* mmd_path = golden_path("render-decorations.mmd");
            char* json_path = golden_path("render-decorations.json");
            char* input = read_file(mmd_path);
            REQUIRE(input != NULL);
            char* expected = read_file(json_path);
            REQUIRE(expected != NULL);

            KanbanDiagram* diagram = kanban_parse(input);
            REQUIRE(diagram != NULL);

            char* actual = kanban_to_json(diagram);
            REQUIRE(actual != NULL);

            check(json_equal(actual, expected), "JSON mismatch");

            json_serialize_free(actual);
            kanban_free_diagram(diagram);
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

            KanbanDiagram* diagram = kanban_parse(input);
            REQUIRE(diagram != NULL);

            char* actual = kanban_to_json(diagram);
            REQUIRE(actual != NULL);

            check(json_equal(actual, expected), "JSON mismatch");

            json_serialize_free(actual);
            kanban_free_diagram(diagram);
            free(expected);
            free(input);
            free(json_path);
            free(mmd_path);
        }
    }
}
