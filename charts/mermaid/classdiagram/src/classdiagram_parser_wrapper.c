#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "classdiagram/classdiagram_ast.h"
#include "classdiagram_parser_gen.h"
#include "turbo_parser.h"

// Lexer and Parser functions (generated)
void *ClassParserAlloc(void *(*mallocProc)(size_t));
void ClassParser(void *yyp, int yymajor, void* yyminor, ClassParserContext *ctx);
void ClassParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void classdiagram_scan(Scanner *s, void *parser, ClassParserContext *ctx);

static void free_members(ClassMember* m) {
    while (m) {
        ClassMember* next = m->next;
        free(m->name);
        free(m->type);
        free(m->return_type);
        free(m);
        m = next;
    }
}

static void free_nodes(ClassNode* n) {
    while (n) {
        ClassNode* next = n->next;
        free(n->name);
        free(n->annotation);
        free_members(n->members);
        free(n);
        n = next;
    }
}

static void free_relationships(ClassRelationship* r) {
    while (r) {
        ClassRelationship* next = r->next;
        free(r->from);
        free(r->to);
        free(r->label);
        free(r->from_cardinality);
        free(r->to_cardinality);
        free(r);
        r = next;
    }
}

void classdiagram_free(ClassDiagram* diagram) {
    if (!diagram) return;
    free(diagram->title);
    free_nodes(diagram->classes);
    free_relationships(diagram->relationships);
    free(diagram);
}

ClassDiagram* classdiagram_parse(const char* input) {
    ClassParserContext ctx;
    ctx.diagram = (ClassDiagram*)malloc(sizeof(ClassDiagram));
    memset(ctx.diagram, 0, sizeof(ClassDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;
    ctx.current_class = NULL;

    void* parser = ClassParserAlloc(malloc);
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    classdiagram_scan(&s, parser, &ctx);
    
    classdiagram_scan(&s, parser, &ctx);
    
    // Explicitly send EOF token as our grammar expects it for the last line
    ClassParser(parser, CLASS_EOF, 0, &ctx);
    // Send 0 to signal end of input to Lemon
    ClassParser(parser, 0, 0, &ctx);
    
    ClassParserFree(parser, free);

    if (ctx.error_count > 0) {
        classdiagram_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

// --- JSON Serialization ---

static const char* visibility_str(ClassVisibility vis) {
    switch(vis) {
        case CLASS_VISIBILITY_PUBLIC: return "+";
        case CLASS_VISIBILITY_PRIVATE: return "-";
        case CLASS_VISIBILITY_PROTECTED: return "#";
        case CLASS_VISIBILITY_INTERNAL: return "~";
        default: return "";
    }
}

static const char* rel_type_str(ClassRelationshipType type) {
    switch(type) {
        case CLASS_REL_INHERITANCE: return "inheritance";
        case CLASS_REL_COMPOSITION: return "composition";
        case CLASS_REL_AGGREGATION: return "aggregation";
        case CLASS_REL_ASSOCIATION: return "association";
        case CLASS_REL_DEPENDENCY: return "dependency";
        case CLASS_REL_REALIZATION: return "realization";
        case CLASS_REL_LINK: return "link";
        default: return "unknown";
    }
}

static json_value_t* serialize_members(ClassMember* m) {
    json_value_t* arr = turbo_json_create_array();
    while (m) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "name", m->name ? m->name : "");
        if (m->type) turbo_json_object_set_string(obj, "type", m->type);
        if (m->return_type) turbo_json_object_set_string(obj, "returnType", m->return_type);
        turbo_json_object_set_string(obj, "visibility", visibility_str(m->visibility));
        turbo_json_object_set_bool(obj, "isStatic", m->is_static);
        turbo_json_object_set_bool(obj, "isAbstract", m->is_abstract);
        turbo_json_object_set_bool(obj, "isMethod", m->is_method);
        turbo_json_array_add(arr, obj);
        m = m->next;
    }
    return arr;
}

static json_value_t* serialize_classes(ClassNode* n) {
    json_value_t* arr = turbo_json_create_array();
    while (n) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "name", n->name ? n->name : "");
        if (n->annotation) turbo_json_object_set_string(obj, "annotation", n->annotation);
        turbo_json_object_add(obj, "members", serialize_members(n->members));
        turbo_json_array_add(arr, obj);
        n = n->next;
    }
    return arr;
}

static json_value_t* serialize_relationships(ClassRelationship* r) {
    json_value_t* arr = turbo_json_create_array();
    while (r) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "from", r->from ? r->from : "");
        turbo_json_object_set_string(obj, "to", r->to ? r->to : "");
        turbo_json_object_set_string(obj, "type", rel_type_str(r->type));
        if (r->label) turbo_json_object_set_string(obj, "label", r->label);
        if (r->from_cardinality) turbo_json_object_set_string(obj, "fromCardinality", r->from_cardinality);
        if (r->to_cardinality) turbo_json_object_set_string(obj, "toCardinality", r->to_cardinality);
        turbo_json_object_set_bool(obj, "isDotted", r->is_dotted);
        turbo_json_array_add(arr, obj);
        r = r->next;
    }
    return arr;
}

char* classdiagram_to_json(ClassDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    if (diagram->title) {
        turbo_json_object_set_string(root, "title", diagram->title);
    }
    turbo_json_object_add(root, "classes", serialize_classes(diagram->classes));
    turbo_json_object_add(root, "relationships", serialize_relationships(diagram->relationships));

    size_t len;
    char* str = turbo_json_serialize_pretty(root, &len);
    turbo_free_json(root);
    return str;
}
