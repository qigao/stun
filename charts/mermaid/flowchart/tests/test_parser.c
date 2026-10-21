#include "tinytest.h"
#define REQUIRE(cond) do { if (!(cond)) { check_true(cond); return; } } while (0)
#include "flowchart/flowchart_parser_wrapper.h"
#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

extern char* flowchart_to_json(FlowchartDiagram* diagram);

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_len = fread(buf, 1, len, f);
    buf[read_len] = '\0';
    fclose(f);
    return buf;
}

static char* load_golden_file(const char* case_name, const char* suffix, const char* ext) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.%s", GOLDEN_DIR, case_name, ext);
    char* data = read_file(path);
    if (data) return data;

    snprintf(path, sizeof(path), "%s/%s.%s.%s", GOLDEN_DIR, case_name, suffix, ext);
    return read_file(path);
}

static void normalize_json(json_value_t* v) {
    if (!v) return;
    json_type_t type = json_type(v);
    if (type == JSON_OBJECT) {
        size_t count = json_object_size(v);
        for (size_t i = 0; i < count; i++) {
            const char* key = json_object_key(v, i);
            json_value_t* val = json_object_value(v, i);
            if (strcmp(key, "shape") == 0 && json_type(val) == JSON_STRING) {
                const char* shape_val = json_string(val);
                if (strcmp(shape_val, "square") == 0) {
                    json_object_set_string(v, key, "rect");
                }
            }
            normalize_json(val);
        }
    } else if (type == JSON_ARRAY) {
        size_t count = json_array_size(v);
        for (size_t i = 0; i < count; i++) {
            normalize_json(json_array_get(v, i));
        }
    }
}

static int json_value_equal(const json_value_t* a, const json_value_t* b) {
    if (!a || !b || json_type(a) != json_type(b)) return a == b;
    switch (json_type(a)) {
        case JSON_NULL:
            return 1;
        case JSON_BOOL:
            return json_bool(a) == json_bool(b);
        case JSON_NUMBER:
            return json_number(a) == json_number(b);
        case JSON_STRING:
            return strcmp(json_string(a), json_string(b)) == 0;
        case JSON_ARRAY: {
            size_t count = json_array_size(a);
            if (count != json_array_size(b)) return 0;
            for (size_t i = 0; i < count; ++i) {
                if (!json_value_equal(json_array_get(a, i),
                                      json_array_get(b, i))) return 0;
            }
            return 1;
        }
        case JSON_OBJECT: {
            size_t count = json_object_size(a);
            if (count != json_object_size(b)) return 0;
            for (size_t i = 0; i < count; ++i) {
                const char* key = json_object_key(a, i);
                if (!json_value_equal(json_object_value(a, i),
                                      json_object_get(b, key))) return 0;
            }
            return 1;
        }
    }
    return 0;
}

static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (!(ja = json_parse(a, strlen(a)))) return 0;
    if (!(jb = json_parse(b, strlen(b)))) {
        json_free(ja);
        return 0;
    }
    
    normalize_json(ja);
    normalize_json(jb);

    int eq = json_value_equal(ja, jb);
    json_free(ja);
    json_free(jb);
    return eq;
}

spec("flowchart_parser") {
    describe("parsing basics") {
        it("should parse LR flowchart with nodes and edges") {
            const char* input = 
                "flowchart LR\n"
                "  A[Start] --> B(Process)\n";

            FlowchartDiagram* diagram = flowchart_parse(input);
            REQUIRE(diagram != NULL);
            check_true(strcmp(diagram->direction, "LR") == 0);
            flowchart_diagram_free(diagram);
        }

        it("distinguishes complete and partial parses") {
            FlowchartDiagram* diagram = NULL;
            check_true(flowchart_parse_ex("flowchart LR\nA[Good label]", &diagram) ==
                       FLOWCHART_PARSE_COMPLETE);
            REQUIRE(diagram != NULL);
            check_true(strcmp(diagram->nodes->label, "Good label") == 0);
            flowchart_diagram_free(diagram);

            diagram = NULL;
            check_true(flowchart_parse_ex("flowchart LR\na --> b --> c", &diagram) ==
                       FLOWCHART_PARSE_COMPLETE);
            REQUIRE(diagram != NULL);
            check_true(strcmp(diagram->nodes->id, "a") == 0);
            check_true(strcmp(diagram->nodes->next->id, "b") == 0);
            check_true(strcmp(diagram->nodes->next->next->id, "c") == 0);
            check_true(strcmp(diagram->edges->from, "a") == 0);
            check_true(strcmp(diagram->edges->to, "b") == 0);
            check_true(strcmp(diagram->edges->next->from, "b") == 0);
            check_true(strcmp(diagram->edges->next->to, "c") == 0);
            flowchart_diagram_free(diagram);

            diagram = NULL;
            check_true(flowchart_parse_ex("flowchart LR\nA[Good] @", &diagram) ==
                       FLOWCHART_PARSE_PARTIAL);
            REQUIRE(diagram != NULL);
            flowchart_diagram_free(diagram);
        }
        it("records classDef styles and serializes class assignments") {
            const char* input =
                "flowchart LR\n"
                "A --> B\n"
                "class A redBg;\n"
                "classDef redBg fill:#622,color: white;\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            REQUIRE(diagram->class_defs != NULL);
            check_true(strcmp(diagram->class_defs->id, "redBg") == 0);
            REQUIRE(diagram->class_defs->styles != NULL);
            check_true(strcmp(diagram->class_defs->styles->key, "fill") == 0);
            check_true(strcmp(diagram->class_defs->styles->value, "#622") == 0);
            REQUIRE(diagram->class_defs->styles->next != NULL);
            check_true(strcmp(diagram->class_defs->styles->next->key, "color") == 0);
            check_true(strcmp(diagram->class_defs->styles->next->value, "white") == 0);
            REQUIRE(diagram->classes != NULL);
            check_true(strcmp(diagram->classes->node_id, "A") == 0);
            check_true(strcmp(diagram->classes->class_name, "redBg") == 0);

            const char* expected =
                "{"
                "\"classDefs\":{\"redBg\":{\"id\":\"redBg\",\"styles\":{\"fill\":\"#622\",\"color\":\"white\"}}},"
                "\"classes\":{\"A\":[\"redBg\"]},"
                "\"clicks\":[],"
                "\"direction\":\"LR\","
                "\"linkStyles\":[],"
                "\"links\":[{\"length\":1,\"source\":\"A\",\"stroke\":\"normal\",\"target\":\"B\",\"type\":\"arrow_point\"}],"
                "\"nodes\":{\"A\":{\"id\":\"A\",\"shape\":\"rect\"},\"B\":{\"id\":\"B\",\"shape\":\"rect\"}},"
                "\"subgraphs\":[],"
                "\"type\":\"flowchart\""
                "}";
            char* actual = flowchart_to_json(diagram);
            int json_matches = actual != NULL && json_equal(actual, expected);
            if (actual) json_serialize_free(actual);
            flowchart_diagram_free(diagram);
            check_true(json_matches);
        }
    }

    describe("subgraph member references") {
        it("preserves node references and duplicate edge endpoints") {
            const char* input =
                "flowchart TB\n"
                "subgraph one\n"
                "a1 --> a2\n"
                "end\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            FlowchartSubGraph* sg = diagram->subgraphs;
            check_true(sg != NULL);
            if (sg) {
                FlowchartNodeRef* ref = sg->node_refs;
                const char* expected[] = {"a2", "a1", "a2", "a1"};
                for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
                    check_true(ref != NULL);
                    if (!ref) break;
                    check_true(ref->id != NULL && strcmp(ref->id, expected[i]) == 0);
                    ref = ref->next;
                }
                check_true(ref == NULL);
            }
            flowchart_diagram_free(diagram);
        }
    }

    describe("repeated edge membership") {
        it("does not register existing nodes as new declarations on every edge") {
            const char* input =
                "flowchart RL\n"
                "subgraph one\n"
                "a1 -- l1 --> a2\n"
                "a1 -- l2 --> a2\n"
                "end\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;
            FlowchartSubGraph* sg = diagram->subgraphs;
            int members_ok = sg && sg->node_refs;
            const char* expected[] = {"a2", "a1", "a2", "a1", "a2", "a1"};
            FlowchartNodeRef* ref = sg ? sg->node_refs : NULL;
            for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
                if (!ref || strcmp(ref->id, expected[i]) != 0) {
                    members_ok = 0;
                    break;
                }
                ref = ref->next;
            }
            check_true(members_ok && ref == NULL);
            flowchart_diagram_free(diagram);
        }
    }

    describe("cross-subgraph node ownership") {
        it("does not transfer an existing node by re-declaration alone") {
            const char* input =
                "flowchart LR\n"
                "subgraph A\n"
                "a --> b\n"
                "end\n"
                "subgraph B\n"
                "b\n"
                "end\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;
            FlowchartSubGraph* first = diagram->subgraphs;
            FlowchartSubGraph* second = first ? first->next : NULL;
            check_true(first && first->id && strcmp(first->id, "A") == 0 &&
                       first->node_refs != NULL);
            check_true(second && second->id && strcmp(second->id, "B") == 0 &&
                       second->node_refs == NULL);
            flowchart_diagram_free(diagram);
        }
    }

    describe("nested subgraph JSON projection") {
        it("emits children before parents in a flat JSON array") {
            const char* input =
                "flowchart LR\n"
                "subgraph TOP\n"
                "subgraph B1\n"
                "a --> b\n"
                "end\n"
                "subgraph B2\n"
                "c --> d\n"
                "end\n"
                "end\n"
                "B1 --> B2\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            FlowchartSubGraph* parent = diagram->subgraphs;
            FlowchartNodeRef* ref = parent ? parent->node_refs : NULL;
            const char* expected_refs[] = {"B1", "B2", "B2", "B1"};
            int refs_ok = parent != NULL;
            for (size_t i = 0; refs_ok && i < 4; ++i) {
                refs_ok = ref && ref->id && strcmp(ref->id, expected_refs[i]) == 0;
                if (refs_ok) ref = ref->next;
            }
            check_true(refs_ok && ref == NULL);

            char* json = flowchart_to_json(diagram);
            json_value_t* root = json ? json_parse(json, strlen(json)) : NULL;
            json_value_t* subgraphs = root ? json_object_get(root, "subgraphs") : NULL;
            int flat_ok = subgraphs && json_array_size(subgraphs) == 3;
            const char* expected[] = {"B1", "B2", "TOP"};
            for (size_t i = 0; flat_ok && i < 3; ++i) {
                json_value_t* sg = json_array_get(subgraphs, i);
                json_value_t* id = sg ? json_object_get(sg, "id") : NULL;
                flat_ok = id && json_type(id) == JSON_STRING &&
                          strcmp(json_string(id), expected[i]) == 0 &&
                          json_object_get(sg, "subgraphs") == NULL;
            }
            check_true(flat_ok);
            json_free(root);
            free(json);
            flowchart_diagram_free(diagram);
        }
    }

    describe("style target graph identity") {
        it("materializes a styled subgraph ID without parsing CSS as nodes") {
            const char* input =
                "flowchart TB\n"
                "subgraph bar[Bar SubGraph]\n"
                "C\n"
                "end\n"
                "style bar fill:#999,stroke-width:10px,stroke:#0F0\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            int seen_bar = 0, seen_c = 0, node_count = 0;
            for (FlowchartNode* n = diagram->nodes; n; n = n->next) {
                node_count++;
                if (strcmp(n->id, "bar") == 0) ++seen_bar;
                if (strcmp(n->id, "C") == 0) ++seen_c;
            }
            check_true(node_count == 2 && seen_bar == 1 && seen_c == 1);
            check_true(diagram->edges == NULL);
            check_true(diagram->subgraphs &&
                       strcmp(diagram->subgraphs->id, "bar") == 0 &&
                       strcmp(diagram->subgraphs->label, "Bar SubGraph") == 0);
            flowchart_diagram_free(diagram);
        }
    }

    describe("variable-width edges") {
        it("retains directed tips and lengths on extended arrows") {
            const char* input =
                "flowchart LR\n"
                "A ----> B\n"
                "C ======> D\n"
                "E --- F\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            char* encoded = flowchart_to_json(diagram);
            json_value_t* root = encoded ? json_parse(encoded, strlen(encoded)) : NULL;
            json_value_t* links = root ? json_object_get(root, "links") : NULL;
            int ok = links && json_array_size(links) == 3;
            const char* types[] = {"arrow_point", "arrow_point", "arrow_open"};
            const char* strokes[] = {"normal", "thick", "normal"};
            const double lengths[] = {3, 5, 2};
            for (size_t i = 0; ok && i < 3; ++i) {
                json_value_t* item = json_array_get(links, i);
                json_value_t* length = item ? json_object_get(item, "length") : NULL;
                json_value_t* type = item ? json_object_get(item, "type") : NULL;
                json_value_t* stroke = item ? json_object_get(item, "stroke") : NULL;
                ok = length && json_type(length) == JSON_NUMBER &&
                     json_number(length) == lengths[i] &&
                     type && json_type(type) == JSON_STRING &&
                     strcmp(json_string(type), types[i]) == 0 &&
                     stroke && json_type(stroke) == JSON_STRING &&
                     strcmp(json_string(stroke), strokes[i]) == 0;
            }
            check_true(ok);
            json_free(root);
            free(encoded);
            flowchart_diagram_free(diagram);
        }
    }

    describe("click statement metadata") {
        it("serializes href and callback clicks in source order") {
            const char* input =
                "flowchart LR\n"
                "A --> B\n"
                "click A \"index.html#link-clicked\" \"link test\"\n"
                "click B testClick \"click test\"\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            if (!diagram) {
                check_true(status == FLOWCHART_PARSE_COMPLETE);
                return;
            }

            FlowchartClick* first_click = diagram->clicks;
            FlowchartClick* second_click = first_click ? first_click->next : NULL;
            int click_ast_matches =
                first_click && first_click->node_id &&
                strcmp(first_click->node_id, "A") == 0 &&
                first_click->is_href == 1 && first_click->target &&
                strcmp(first_click->target, "index.html#link-clicked") == 0 &&
                second_click && second_click->node_id &&
                strcmp(second_click->node_id, "B") == 0 &&
                second_click->is_href == 0 && second_click->target &&
                strcmp(second_click->target, "testClick") == 0 &&
                second_click->next == NULL;

            const char* expected =
                "{"
                "\"classDefs\":{},\"classes\":{},"
                "\"clicks\":[{\"href\":\"index.html#link-clicked\",\"nodeId\":\"A\"},"
                "{\"callback\":\"testClick\",\"nodeId\":\"B\"}],"
                "\"direction\":\"LR\",\"linkStyles\":[],"
                "\"links\":[{\"length\":1,\"source\":\"A\",\"stroke\":\"normal\","
                "\"target\":\"B\",\"type\":\"arrow_point\"}],"
                "\"nodes\":{\"A\":{\"id\":\"A\",\"shape\":\"rect\"},"
                "\"B\":{\"id\":\"B\",\"shape\":\"rect\"}},"
                "\"subgraphs\":[],\"type\":\"flowchart\""
                "}";
            char* actual = flowchart_to_json(diagram);
            int click_json_matches = actual != NULL && json_equal(actual, expected);
            if (actual) json_serialize_free(actual);
            flowchart_diagram_free(diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            check_true(click_ast_matches);
            check_true(click_json_matches);
        }
    }

    describe("ampersand edge groups") {
        it("expands each adjacent node group as a Cartesian product") {
            const char* input =
                "flowchart LR\n"
                "a --> b & c --> d & e\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            const char* expected[][2] = {
                {"a", "b"}, {"a", "c"},
                {"b", "d"}, {"b", "e"},
                {"c", "d"}, {"c", "e"}
            };
            int seen[6] = {0};
            int edge_count = 0;
            for (FlowchartEdge* edge = diagram->edges; edge; edge = edge->next) {
                edge_count++;
                for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
                    if (strcmp(edge->from, expected[i][0]) == 0 &&
                        strcmp(edge->to, expected[i][1]) == 0)
                        seen[i]++;
                }
            }
            check_true(edge_count == 6);
            for (size_t i = 0; i < sizeof(seen) / sizeof(seen[0]); ++i)
                check_true(seen[i] == 1);
            flowchart_diagram_free(diagram);
        }
    }


    describe("inline Mermaid edge labels") {
        it("preserves the edge label without introducing phantom nodes") {
            const char* input =
                "flowchart LR\n"
                "a1 -- l1 --> a2\n"
                "i1 --lb1--> f1\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            FlowchartEdge* first = diagram->edges;
            FlowchartEdge* second = first ? first->next : NULL;
            int edges_ok =
                first && first->from && strcmp(first->from, "a1") == 0 &&
                first->to && strcmp(first->to, "a2") == 0 &&
                first->label && strcmp(first->label, "l1") == 0 &&
                second && second->from && strcmp(second->from, "i1") == 0 &&
                second->to && strcmp(second->to, "f1") == 0 &&
                second->label && strcmp(second->label, "lb1") == 0 &&
                second->next == NULL;
            int node_count = 0;
            for (FlowchartNode* n = diagram->nodes; n; n = n->next) ++node_count;
            check_true(edges_ok);
            check_true(node_count == 4);
            flowchart_diagram_free(diagram);
        }
    }

    describe("punctuated Mermaid identifiers") {
        it("preserves dotted and hyphenated IDs without swallowing arrows") {
            const char* input =
                "flowchart LR\n"
                "default-index.js --> dot.template.js\n"
                "index.js --> module-utl.js\n"
                "sid-AAA-BBB --> P1.5\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            const char* expected[][2] = {
                {"default-index.js", "dot.template.js"},
                {"index.js", "module-utl.js"},
                {"sid-AAA-BBB", "P1.5"}
            };
            FlowchartEdge* edge = diagram->edges;
            int edges_ok = 1;
            for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
                if (!edge || !edge->from || !edge->to ||
                    strcmp(edge->from, expected[i][0]) != 0 ||
                    strcmp(edge->to, expected[i][1]) != 0) {
                    edges_ok = 0;
                    break;
                }
                edge = edge->next;
            }
            check_true(edges_ok && edge == NULL);
            flowchart_diagram_free(diagram);
        }
    }

    describe("unquoted comma labels") {
        it("retains commas between text and HTML line breaks") {
            const char* input =
                "flowchart LR\n"
                "A[work,<br />something,else] --> B\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;
            FlowchartNode* a = diagram->nodes;
            while (a && strcmp(a->id, "A") != 0) a = a->next;
            check_true(a && a->label &&
                       strcmp(a->label, "work,<br />something,else") == 0);
            flowchart_diagram_free(diagram);
        }
    }


    describe("circle and cylinder shape delimiters") {
        it("distinguishes circles double circles cylinders and empty circles") {
            const char* input =
                "flowchart LR\n"
                "A((circle)) --> B(((double)))\n"
                "B --> C[(storage)]\n"
                "D(( )) --> C\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            FlowchartNode* a = NULL;
            FlowchartNode* b = NULL;
            FlowchartNode* c = NULL;
            FlowchartNode* d = NULL;
            for (FlowchartNode* n = diagram->nodes; n; n = n->next) {
                if (strcmp(n->id, "A") == 0) a = n;
                else if (strcmp(n->id, "B") == 0) b = n;
                else if (strcmp(n->id, "C") == 0) c = n;
                else if (strcmp(n->id, "D") == 0) d = n;
            }
            check_true(a && a->shape == FC_SHAPE_CIRCLE &&
                       strcmp(a->label, "circle") == 0);
            check_true(b && b->shape == FC_SHAPE_DOUBLECIRCLE &&
                       strcmp(b->label, "double") == 0);
            check_true(c && c->shape == FC_SHAPE_CYLINDER &&
                       strcmp(c->label, "storage") == 0);
            check_true(d && d->shape == FC_SHAPE_CIRCLE &&
                       strcmp(d->label, " ") == 0);
            flowchart_diagram_free(diagram);
        }
    }

    describe("modern node declaration syntax") {
        it("accepts ampersand-separated and annotated nodes") {
            const char* input =
                "flowchart TB\n"
                "n2[\"label for n2\"] & n4@{ label: \"label for n4\" }"
                " & n5@{ label: \"label for n5\" }\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            int seen_n2 = 0, seen_n4 = 0, seen_n5 = 0;
            for (FlowchartNode* node = diagram->nodes; node; node = node->next) {
                if (strcmp(node->id, "n2") == 0) {
                    seen_n2++;
                    check_true(strcmp(node->label, "label for n2") == 0);
                } else if (strcmp(node->id, "n4") == 0) {
                    seen_n4++;
                    check_true(strcmp(node->label, "n4") == 0);
                } else if (strcmp(node->id, "n5") == 0) {
                    seen_n5++;
                    check_true(strcmp(node->label, "n5") == 0);
                }
            }
            check_true(seen_n2 == 1);
            check_true(seen_n4 == 1);
            check_true(seen_n5 == 1);
            check_true(diagram->edges == NULL);
            flowchart_diagram_free(diagram);
        }
    }

    describe("multi-index linkStyle") {
        it("retains each selected index and its independent style ownership") {
            const char* input =
                "flowchart TD\n"
                "A --> B\n"
                "A --> C\n"
                "linkStyle 0,1 color:orange, stroke:orange;\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;
            FlowchartLinkStyle* first = diagram->link_styles;
            FlowchartLinkStyle* second = first ? first->next : NULL;
            int owned_ok =
                first && strcmp(first->index, "0") == 0 &&
                first->styles && first->styles->next &&
                second && strcmp(second->index, "1") == 0 &&
                second->styles && second->styles->next &&
                first->styles != second->styles &&
                first->styles->key && second->styles->key &&
                strcmp(first->styles->key, "color") == 0 &&
                strcmp(second->styles->key, "color") == 0 &&
                second->next == NULL;
            check_true(owned_ok);

            char* encoded = flowchart_to_json(diagram);
            json_value_t* root = encoded ? json_parse(encoded, strlen(encoded)) : NULL;
            json_value_t* styles = root ? json_object_get(root, "linkStyles") : NULL;
            int json_ok = styles && json_array_size(styles) == 2;
            if (json_ok) {
                json_value_t* first_index =
                    json_object_get(json_array_get(styles, 0), "index");
                json_value_t* second_index =
                    json_object_get(json_array_get(styles, 1), "index");
                json_ok = first_index && second_index &&
                          json_type(first_index) == JSON_NUMBER &&
                          json_type(second_index) == JSON_NUMBER &&
                          json_number(first_index) == 0 &&
                          json_number(second_index) == 1;
            }
            check_true(json_ok);
            json_free(root);
            free(encoded);
            flowchart_diagram_free(diagram);
        }
    }

    describe("linkStyle metadata") {
        it("serializes default and indexed styles without consuming later nodes") {
            const char* input =
                "flowchart LR\n"
                "A --> B\n"
                "A --> C\n"
                "linkStyle default color:Sienna\n"
                "linkStyle 0 stroke:#D50000;\n"
                "style A stroke:#ff0000,fill:#ffcccc\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            if (!diagram) {
                check_true(status == FLOWCHART_PARSE_COMPLETE);
                return;
            }

            FlowchartLinkStyle* first_item = diagram->link_styles;
            FlowchartLinkStyle* second_item = first_item ? first_item->next : NULL;
            int ast_styles_ok =
                first_item && first_item->index &&
                strcmp(first_item->index, "default") == 0 &&
                first_item->styles && first_item->styles->key &&
                strcmp(first_item->styles->key, "color") == 0 &&
                first_item->styles->value &&
                strcmp(first_item->styles->value, "Sienna") == 0 &&
                first_item->styles->next == NULL &&
                second_item && second_item->index &&
                strcmp(second_item->index, "0") == 0 &&
                second_item->styles && second_item->styles->key &&
                strcmp(second_item->styles->key, "stroke") == 0 &&
                second_item->styles->value &&
                strcmp(second_item->styles->value, "#D50000") == 0 &&
                second_item->styles->next == NULL &&
                second_item->next == NULL;
            FlowchartNode* node_a = diagram->nodes;
            FlowchartNode* node_b = node_a ? node_a->next : NULL;
            FlowchartNode* node_c = node_b ? node_b->next : NULL;
            int nodes_ok = node_a && node_a->id && strcmp(node_a->id, "A") == 0 &&
                           node_b && node_b->id && strcmp(node_b->id, "B") == 0 &&
                           node_c && node_c->id && strcmp(node_c->id, "C") == 0 &&
                           node_c->next == NULL;

            char* actual = flowchart_to_json(diagram);
            json_value_t* root = actual ? json_parse(actual, strlen(actual)) : NULL;
            json_value_t* styles = root ? json_object_get(root, "linkStyles") : NULL;
            int json_styles_ok = styles && json_array_size(styles) == 2;
            if (json_styles_ok) {
                json_value_t* first = json_array_get(styles, 0);
                json_value_t* second = json_array_get(styles, 1);
                json_value_t* first_index = first ? json_object_get(first, "index") : NULL;
                json_value_t* second_index = second ? json_object_get(second, "index") : NULL;
                json_value_t* first_style_obj = first ? json_object_get(first, "styles") : NULL;
                json_value_t* second_style_obj = second ? json_object_get(second, "styles") : NULL;
                json_value_t* color = first_style_obj ? json_object_get(first_style_obj, "color") : NULL;
                json_value_t* stroke = second_style_obj ? json_object_get(second_style_obj, "stroke") : NULL;
                json_styles_ok = first_index && json_type(first_index) == JSON_STRING &&
                    strcmp(json_string(first_index), "default") == 0 &&
                    second_index && json_type(second_index) == JSON_NUMBER &&
                    json_number(second_index) == 0.0 &&
                    color && json_type(color) == JSON_STRING &&
                    strcmp(json_string(color), "Sienna") == 0 &&
                    stroke && json_type(stroke) == JSON_STRING &&
                    strcmp(json_string(stroke), "#D50000") == 0;
            }
            json_free(root);
            if (actual) json_serialize_free(actual);
            flowchart_diagram_free(diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            check_true(ast_styles_ok);
            check_true(nodes_ok);
            check_true(json_styles_ok);
        }
    }

    describe("unquoted label punctuation") {
        it("preserves periods, exclamation marks, and question marks") {
            const char* input =
                "flowchart LR\n"
                "A[Wait... really?] --> B[No!]\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            FlowchartNode* a = diagram->nodes;
            while (a && strcmp(a->id, "A") != 0) a = a->next;
            FlowchartNode* b = diagram->nodes;
            while (b && strcmp(b->id, "B") != 0) b = b->next;
            check_true(a != NULL);
            check_true(b != NULL);
            if (a) check_true(strcmp(a->label, "Wait... really?") == 0);
            if (b) check_true(strcmp(b->label, "No!") == 0);
            flowchart_diagram_free(diagram);
        }
    }

    describe("legacy flowchart shape syntax") {
        it("parses odd and leaning parallelogram nodes") {
            const char* input =
                "flowchart LR\n"
                "G>red text] --> J[/blue text/]\n";
            FlowchartDiagram* diagram = NULL;
            FlowchartParseStatus status = flowchart_parse_ex(input, &diagram);
            check_true(status == FLOWCHART_PARSE_COMPLETE);
            if (!diagram) return;

            FlowchartNode* g = diagram->nodes;
            while (g && strcmp(g->id, "G") != 0) g = g->next;
            FlowchartNode* j = diagram->nodes;
            while (j && strcmp(j->id, "J") != 0) j = j->next;
            check_true(g != NULL);
            check_true(j != NULL);
            if (g) {
                check_true(g->shape == FC_SHAPE_ODD);
                check_true(strcmp(g->label, "red text") == 0);
            }
            if (j) {
                check_true(j->shape == FC_SHAPE_TRAPEZOID);
                check_true(strcmp(j->label, "blue text") == 0);
            }
            flowchart_diagram_free(diagram);
        }
    }

    describe("golden tests") {
        int count = 0;
        int golden_failures = 0;
#ifdef _WIN32
        char search_path[512];
        snprintf(search_path, sizeof(search_path), "%s/*.input.mmd", GOLDEN_DIR);
        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(search_path, &find_data);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                const char* filename = find_data.cFileName;
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext) continue;
                size_t name_len = (size_t)(ext - filename);
                if (name_len >= 128) continue;
                char case_name[128];
                memcpy(case_name, filename, name_len);
                case_name[name_len] = '\0';
                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd");
                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");
                    int failed = !input || !expected;
                    FlowchartDiagram* diagram = NULL;
                    char* actual = NULL;
                    if (!failed) {
                        diagram = flowchart_parse(input);
                        if (!diagram) {
                            failed = 1;
                        } else {
                            actual = flowchart_to_json(diagram);
                            if (!actual || !json_equal(actual, expected))
                                failed = 1;
                        }
                    }
                    /* Diagnostics must run before releasing their source buffers. */
                    /* Defer assertions until the directory handle is closed. */
                    if (failed) {
                        fprintf(stderr, "Flowchart golden mismatch: %s\n", case_name);
                        if (golden_failures < 3) {
                            fprintf(stderr, "--- expected JSON (%s) ---\n%s\n",
                                    case_name, expected ? expected : "<missing>");
                            fprintf(stderr, "--- actual JSON (%s) ---\n%s\n",
                                    case_name, actual ? actual : "<parse/serialize failed>");
                        }
                        golden_failures++;
                    }
                    if (actual) json_serialize_free(actual);
                    if (diagram) flowchart_diagram_free(diagram);
                    free(expected);
                    free(input);
                }
                if (++count >= 1000) break;
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR* directory = opendir(GOLDEN_DIR);
        check_true(directory != NULL);
        if (directory) {
            struct dirent* entry;
            while ((entry = readdir(directory)) != NULL) {
                const char* filename = entry->d_name;
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext || ext[strlen(".input.mmd")] != '\0') continue;
                size_t name_len = (size_t)(ext - filename);
                if (name_len >= 128) continue;
                char case_name[128];
                memcpy(case_name, filename, name_len);
                case_name[name_len] = '\0';
                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd");
                    char* expected = load_golden_file(case_name, "output", "json");
                    if (!expected) expected = load_golden_file(case_name, "", "json");
                    int failed = !input || !expected;
                    FlowchartDiagram* diagram = NULL;
                    char* actual = NULL;
                    if (!failed) {
                        diagram = flowchart_parse(input);
                        if (!diagram) {
                            failed = 1;
                        } else {
                            actual = flowchart_to_json(diagram);
                            if (!actual || !json_equal(actual, expected))
                                failed = 1;
                        }
                    }
                    /* Diagnostics must run before releasing their source buffers. */
                    /* Defer assertions until the directory handle is closed. */
                    if (failed) {
                        fprintf(stderr, "Flowchart golden mismatch: %s\n", case_name);
                        if (golden_failures < 3) {
                            fprintf(stderr, "--- expected JSON (%s) ---\n%s\n",
                                    case_name, expected ? expected : "<missing>");
                            fprintf(stderr, "--- actual JSON (%s) ---\n%s\n",
                                    case_name, actual ? actual : "<parse/serialize failed>");
                        }
                        golden_failures++;
                    }
                    if (actual) json_serialize_free(actual);
                    if (diagram) flowchart_diagram_free(diagram);
                    free(expected);
                    free(input);
                }
                if (++count >= 1000) break;
            }
            closedir(directory);
        }
#endif
        check_true(golden_failures == 0);
        printf("Ran %d dynamic golden tests.\n", count);
    }
}