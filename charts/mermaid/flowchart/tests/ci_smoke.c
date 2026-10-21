/* Fast, SDK-independent conformance checks for the production AST parser.
 * These are not JSON golden tests and never replace full runtime acceptance.
 */
#include "flowchart/flowchart_parser_wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *from;
    const char *to;
    const char *label;
} ExpectedEdge;

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "flowchart smoke %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static FlowchartNode *find_node(FlowchartDiagram *diagram, const char *id) {
    for (FlowchartNode *n = diagram->nodes; n; n = n->next) {
        if (n->id && strcmp(n->id, id) == 0)
            return n;
    }
    return NULL;
}

static void check_edges(const char *name, const char *source,
                        const ExpectedEdge *edges, size_t expected_count,
                        size_t expected_node_count) {
    FlowchartDiagram *diagram = NULL;
    FlowchartParseStatus status = flowchart_parse_ex(source, &diagram);
    if (status != FLOWCHART_PARSE_COMPLETE || !diagram) {
        fprintf(stderr, "%s: parse status=%d, error=%s\n",
                name, (int)status, flowchart_get_last_error());
        ++failures;
        flowchart_diagram_free(diagram);
        return;
    }

    size_t actual_node_count = 0;
    size_t actual_edge_count = 0;
    for (FlowchartNode *n = diagram->nodes; n; n = n->next)
        ++actual_node_count;
    CHECK(actual_node_count == expected_node_count);
    for (FlowchartEdge *e = diagram->edges; e; e = e->next) {
        if (actual_edge_count >= expected_count) {
            fprintf(stderr, "%s: unexpected extra edge %s -> %s\n",
                    name, e->from ? e->from : "", e->to ? e->to : "");
            ++failures;
        } else {
            const ExpectedEdge *expected = &edges[actual_edge_count];
            if (!e->from || !e->to ||
                strcmp(e->from, expected->from) != 0 ||
                strcmp(e->to, expected->to) != 0 ||
                ((e->label == NULL) != (expected->label == NULL)) ||
                (e->label && strcmp(e->label, expected->label) != 0)) {
                fprintf(stderr,
                        "%s edge %zu: expected %s -> %s (%s), got %s -> %s (%s)\n",
                        name, actual_edge_count,
                        expected->from, expected->to,
                        expected->label ? expected->label : "",
                        e->from ? e->from : "", e->to ? e->to : "",
                        e->label ? e->label : "");
                ++failures;
            }
        }
        ++actual_edge_count;
    }
    CHECK(actual_edge_count == expected_count);
    flowchart_diagram_free(diagram);
}

static void test_ampersand_groups(void) {
    static const ExpectedEdge edges[] = {
        {"a", "b", NULL},
        {"a", "c", NULL},
        {"b", "d", NULL},
        {"b", "e", NULL},
        {"c", "d", NULL},
        {"c", "e", NULL},
    };
    check_edges("ampersand groups",
                "flowchart LR\n"
                "a --> b & c --> d & e\n",
                edges, sizeof(edges) / sizeof(edges[0]), 5);
}

static void test_inline_edge_labels(void) {
    static const ExpectedEdge edges[] = {
        {"a1", "a2", "l1"},
        {"i1", "f1", "lb1"},
        {"f1", "a1", "quoted label"},
    };
    check_edges("inline edge labels",
                "flowchart LR\n"
                "a1 -- l1 --> a2\n"
                "i1 --lb1-->f1\n"
                "f1 -->|quoted label| a1\n",
                edges, sizeof(edges) / sizeof(edges[0]), 4);
}

static void test_punctuated_identifiers(void) {
    static const ExpectedEdge edges[] = {
        {"default-index.js", "dot.template.js", NULL},
        {"index.js", "module-utl.js", NULL},
        {"sid-AAA-BBB", "P1.5", NULL},
    };
    check_edges("punctuated IDs",
                "flowchart LR\n"
                "default-index.js --> dot.template.js\n"
                "index.js --> module-utl.js\n"
                "sid-AAA-BBB --> P1.5\n",
                edges, sizeof(edges) / sizeof(edges[0]), 6);
}

static void test_shapes_and_text(void) {
    FlowchartDiagram *diagram = NULL;
    const char *source =
        "flowchart LR\n"
        "A((circle)) --> B(((double)))\n"
        "B --> C[(storage)]\n"
        "D(( )) --> C\n"
        "Q[work,<br />something,else]\n"
        "Z[ h ]\n"
        "W(( A ))\n";

    FlowchartParseStatus status = flowchart_parse_ex(source, &diagram);
    CHECK(status == FLOWCHART_PARSE_COMPLETE);
    if (!diagram) { ++failures; return; }

    FlowchartNode *a = find_node(diagram, "A");
    FlowchartNode *b = find_node(diagram, "B");
    FlowchartNode *c = find_node(diagram, "C");
    FlowchartNode *d = find_node(diagram, "D");
    FlowchartNode *q = find_node(diagram, "Q");
    FlowchartNode *z = find_node(diagram, "Z");
    FlowchartNode *w = find_node(diagram, "W");
    CHECK(a && a->shape == FC_SHAPE_CIRCLE &&
          a->label && strcmp(a->label, "circle") == 0);
    CHECK(b && b->shape == FC_SHAPE_DOUBLECIRCLE &&
          b->label && strcmp(b->label, "double") == 0);
    CHECK(c && c->shape == FC_SHAPE_CYLINDER &&
          c->label && strcmp(c->label, "storage") == 0);
    CHECK(d && d->shape == FC_SHAPE_CIRCLE &&
          d->label && strcmp(d->label, " ") == 0);
    CHECK(q && q->label && strcmp(q->label, "work,<br />something,else") == 0);
    CHECK(z && z->label && strcmp(z->label, " h ") == 0);
    CHECK(w && w->label && strcmp(w->label, " A ") == 0);

    flowchart_diagram_free(diagram);
}

static void test_malformed_input(void) {
    FlowchartDiagram *diagram = NULL;
    CHECK(flowchart_parse_ex(NULL, &diagram) == FLOWCHART_PARSE_ERROR);
    CHECK(diagram == NULL);
    CHECK(flowchart_parse_ex("flowchart LR\nA -->", &diagram) !=
          FLOWCHART_PARSE_COMPLETE);
    flowchart_diagram_free(diagram);
}

int main(void) {
    test_ampersand_groups();
    test_inline_edge_labels();
    test_punctuated_identifiers();
    test_shapes_and_text();
    test_malformed_input();
    if (failures) {
        fprintf(stderr, "Flowchart native AST smoke: %d assertion(s) failed\n",
                failures);
        return EXIT_FAILURE;
    }
    puts("Flowchart native AST smoke: all checks passed");
    return EXIT_SUCCESS;
}
