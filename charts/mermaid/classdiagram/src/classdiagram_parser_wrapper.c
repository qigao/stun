#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "classdiagram/classdiagram_ast.h"
#include "classdiagram_parser_gen.h"
#include "json_parser.h"

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
    if (!input) return NULL;
    ClassParserContext ctx;
    ctx.diagram = (ClassDiagram*)malloc(sizeof(ClassDiagram));
    if (!ctx.diagram) return NULL;
    memset(ctx.diagram, 0, sizeof(ClassDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;
    ctx.current_class = NULL;

    void* parser = ClassParserAlloc(malloc);
    if (!parser) {
        classdiagram_free(ctx.diagram);
        return NULL;
    }
    
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
    json_value_t* arr = json_create_array();
    while (m) {
        json_value_t* obj = json_create_object();
        json_object_set_string(obj, "name", m->name ? m->name : "");
        if (m->type) json_object_set_string(obj, "type", m->type);
        if (m->return_type) json_object_set_string(obj, "returnType", m->return_type);
        json_object_set_string(obj, "visibility", visibility_str(m->visibility));
        json_object_set_bool(obj, "isStatic", m->is_static);
        json_object_set_bool(obj, "isAbstract", m->is_abstract);
        json_object_set_bool(obj, "isMethod", m->is_method);
        json_array_add(arr, obj);
        m = m->next;
    }
    return arr;
}

static json_value_t* serialize_classes(ClassNode* n) {
    json_value_t* arr = json_create_array();
    while (n) {
        json_value_t* obj = json_create_object();
        json_object_set_string(obj, "name", n->name ? n->name : "");
        if (n->annotation) json_object_set_string(obj, "annotation", n->annotation);
        json_object_add(obj, "members", serialize_members(n->members));
        json_array_add(arr, obj);
        n = n->next;
    }
    return arr;
}

static json_value_t* serialize_relationships(ClassRelationship* r) {
    json_value_t* arr = json_create_array();
    while (r) {
        json_value_t* obj = json_create_object();
        json_object_set_string(obj, "from", r->from ? r->from : "");
        json_object_set_string(obj, "to", r->to ? r->to : "");
        json_object_set_string(obj, "type", rel_type_str(r->type));
        if (r->label) json_object_set_string(obj, "label", r->label);
        if (r->from_cardinality) json_object_set_string(obj, "fromCardinality", r->from_cardinality);
        if (r->to_cardinality) json_object_set_string(obj, "toCardinality", r->to_cardinality);
        json_object_set_bool(obj, "isDotted", r->is_dotted);
        json_array_add(arr, obj);
        r = r->next;
    }
    return arr;
}

char* classdiagram_to_json(ClassDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = json_create_object();
    if (diagram->title) {
        json_object_set_string(root, "title", diagram->title);
    }
    json_object_add(root, "classes", serialize_classes(diagram->classes));
    json_object_add(root, "relationships", serialize_relationships(diagram->relationships));

    size_t len;
    char* str = json_serialize_pretty(root, &len);
    json_free(&root);
    return str;
}
