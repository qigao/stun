#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "turbo_parser.h"

#include "statediagram/statediagram_ast.h"
#include "statediagram_parser_gen.h"

// Lexer and Parser functions (generated)
void *StateParserAlloc(void *(*mallocProc)(size_t));
void StateParser(void *yyp, int yymajor, void* yyminor, StateParserContext *ctx);
void StateParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void statediagram_scan(Scanner *s, void *parser, StateParserContext *ctx);

static void free_nodes(StateNode* n);
static void free_transitions(StateTransition* t);

static void free_doc(StateDoc* doc) {
    if (!doc) return;
    free_nodes(doc->nodes);
    free_transitions(doc->transitions);
    free(doc);
}

static void free_nodes(StateNode* n) {
    while (n) {
        StateNode* next = n->next;
        free(n->id);
        free(n->description);
        if (n->doc) free_doc(n->doc);
        free(n);
        n = next;
    }
}

static void free_transitions(StateTransition* t) {
    while (t) {
        StateTransition* next = t->next;
        free(t->id1);
        free(t->id2);
        free(t->description);
        free(t);
        t = next;
    }
}

void statediagram_free(StateDiagram* diagram) {
    if (!diagram) return;
    free(diagram->title);
    free(diagram->direction);
    free_doc(diagram->root);
    free(diagram);
}

StateDiagram* statediagram_parse(const char* input) {
    StateParserContext ctx;
    ctx.diagram = (StateDiagram*)malloc(sizeof(StateDiagram));
    memset(ctx.diagram, 0, sizeof(StateDiagram));
    ctx.diagram->root = (StateDoc*)malloc(sizeof(StateDoc));
    memset(ctx.diagram->root, 0, sizeof(StateDoc));
    
    ctx.current_doc = ctx.diagram->root;
    ctx.doc_stack = NULL;
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = StateParserAlloc(malloc);
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    statediagram_scan(&s, parser, &ctx);
    
    // Send trailing NL
    StateParser(parser, STATE_NL, NULL, &ctx);
    // Send EOF
    StateParser(parser, 0, NULL, &ctx);
    StateParserFree(parser, free);

    if (ctx.error_count > 0) {
        statediagram_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}


static json_value_t* serialize_nodes(StateNode* n);
static json_value_t* serialize_transitions(StateTransition* t);

static json_value_t* serialize_doc(StateDoc* doc) {
    if (!doc) return NULL;
    json_value_t* obj = turbo_json_create_object();
    if (doc->nodes) {
        turbo_json_object_add(obj, "nodes", serialize_nodes(doc->nodes));
    }
    if (doc->transitions) {
        turbo_json_object_add(obj, "transitions", serialize_transitions(doc->transitions));
    }
    return obj;
}

static json_value_t* serialize_nodes(StateNode* n) {
    json_value_t* arr = turbo_json_create_array();
    while (n) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "id", n->id ? n->id : "");
        if (n->description) {
            turbo_json_object_set_string(obj, "description", n->description);
        }
        if (n->doc) {
            turbo_json_object_add(obj, "doc", serialize_doc(n->doc));
        }
        turbo_json_array_add(arr, obj);
        n = n->next;
    }
    return arr;
}

static json_value_t* serialize_transitions(StateTransition* t) {
    json_value_t* arr = turbo_json_create_array();
    while (t) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "id1", t->id1 ? t->id1 : "");
        turbo_json_object_set_string(obj, "id2", t->id2 ? t->id2 : "");
        if (t->description) {
            turbo_json_object_set_string(obj, "description", t->description);
        }
        turbo_json_array_add(arr, obj);
        t = t->next;
    }
    return arr;
}

char* statediagram_to_json(StateDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    turbo_json_object_set_string(root, "type", "stateDiagram");

    if (diagram->title) {
        turbo_json_object_set_string(root, "title", diagram->title);
    }

    if (diagram->root) {
        turbo_json_object_add(root, "root", serialize_doc(diagram->root));
    }

    size_t len;
    return turbo_json_serialize_pretty_crlf(root, &len);
}
