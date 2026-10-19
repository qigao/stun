#include "tinytest.h"
#include "mindmap/mindmap_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "json_parser.h"

#ifndef REQUIRE
#define REQUIRE(cond) do { if (!(cond)) { check(0, #cond); return; } } while (0)
#endif

extern MindmapDiagram* mindmap_parse(const char* input);
extern void mindmap_free(MindmapDiagram* diagram);

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

spec("mindmap_parser") {
    describe("parsing basics") {
        it("should parse simple root node") {
            const char* input = "mindmap\n  Root\n";
            MindmapDiagram* diagram = mindmap_parse(input);
            REQUIRE(diagram != NULL);
            REQUIRE(diagram->root != NULL);
            check_true(diagram->root->label != NULL && strcmp(diagram->root->label, "Root") == 0);
            check_true((diagram->root->level) == (2)); 
            mindmap_free(diagram);
        }

        it("should parse hierarchy with indentation") {
             const char* input = 
                 "mindmap\n"
                 "  Root\n"
                 "    Child1\n"
                 "    Child2\n"
                 "      GrandChild\n";
 
             MindmapDiagram* diagram = mindmap_parse(input);
             REQUIRE(diagram != NULL);
             
             MindmapNode* root = diagram->root;
             REQUIRE(root != NULL);
             check_true(root->label != NULL && strcmp(root->label, "Root") == 0);
             
             MindmapNode* child1 = root->children;
             REQUIRE(child1 != NULL);
             check_true(child1->label != NULL && strcmp(child1->label, "Child1") == 0);
             
             MindmapNode* child2 = child1->next;
             REQUIRE(child2 != NULL);
             check_true(child2->label != NULL && strcmp(child2->label, "Child2") == 0);
             
             MindmapNode* grandchild = child2->children;
             REQUIRE(grandchild != NULL);
             check_true(grandchild->label != NULL && strcmp(grandchild->label, "GrandChild") == 0);
             
             mindmap_free(diagram);
        }

        it("should parse shapes") {
             const char* input = 
                 "mindmap\n"
                 "  root((CircleRoot))\n"; 
 
             MindmapDiagram* diagram = mindmap_parse(input);
             REQUIRE(diagram != NULL);
             
             MindmapNode* root = diagram->root;
             REQUIRE(root != NULL);
             check_true(root->id != NULL && strcmp(root->id, "root") == 0);
             check_true(root->label != NULL && strcmp(root->label, "CircleRoot") == 0);
             REQUIRE(root->type == MINDMAP_NODE_CIRCLE);
             
             mindmap_free(diagram);
        }

        it("should parse icons and classes") {
             const char* input = 
                 "mindmap\n"
                 "  Root\n"
                 "  ::icon(fa fa-book)\n"
                 "  :::urgent\n";
 
             MindmapDiagram* diagram = mindmap_parse(input);
             REQUIRE(diagram != NULL);
             
             MindmapNode* root = diagram->root;
             REQUIRE(root != NULL);
             
             // Icon
             REQUIRE(root->icon != NULL);
             check_true(root->icon != NULL && strcmp(root->icon, "fa fa-book") == 0);
             
             // Class
             REQUIRE(root->classes != NULL);
             check_true(root->classes != NULL && strcmp(root->classes, "urgent") == 0);
             
             mindmap_free(diagram);
        }
    }

    describe("golden tests") {
         it("render-basic") {
             char* mmd_path = golden_path("render-basic.mmd");
             char* json_path = golden_path("render-basic.json");
             char* input = read_file(mmd_path);
             REQUIRE(input != NULL);
             char* expected = read_file(json_path);
             REQUIRE(expected != NULL);

             MindmapDiagram* diagram = mindmap_parse(input);
             REQUIRE(diagram != NULL);

             char* actual = mindmap_to_json(diagram);
             REQUIRE(actual != NULL);

             check(json_equal(actual, expected), "JSON mismatch");

             json_serialize_free(actual);
             mindmap_free(diagram);
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

             MindmapDiagram* diagram = mindmap_parse(input);
             REQUIRE(diagram != NULL);

             char* actual = mindmap_to_json(diagram);
             REQUIRE(actual != NULL);

             check(json_equal(actual, expected), "JSON mismatch");

             json_serialize_free(actual);
             mindmap_free(diagram);
             free(expected);
             free(input);
             free(json_path);
             free(mmd_path);
         }
    }
}
