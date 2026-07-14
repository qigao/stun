#include "c4_parser_wrapper.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward decls for Lemon-generated parser
void* C4ParserAlloc(void *(*mallocProc)(size_t));
void C4ParserFree(void *p, void (*freeProc)(void*));
void C4Parser(void *yyp, int yymajor, char* yyminor, C4ParserContext *ctx);

// Forward decl for Lexer
typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
    int at_bol;
} Scanner;
void c4_scan(Scanner *s, void *parser, C4ParserContext *ctx);

// --- AST Helper Functions ---

static char* copy_attr(C4Attribute* attrs, int index) {
    int i = 0;
    while (attrs) {
        if (i == index) return (attrs->value ? strdup(attrs->value) : NULL);
        attrs = attrs->next;
        i++;
    }
    return NULL;
}

C4Element* c4_create_element(C4ElementType type, C4Attribute* attrs) {
    C4Element* el = (C4Element*)calloc(1, sizeof(C4Element));
    el->type = type;
    
    // Mapping attributes based on type.
    // Most C4 elements: alias, label, ?descr, ?techn, ?descr2
    // Boundaries: alias, label, ?type, ?tags, ?link
    
    // Simplification:
    el->alias = copy_attr(attrs, 0);
    el->label = copy_attr(attrs, 1);

    // Attribute mapping based on type
    switch(type) {
        case C4_EL_CONTAINER:
        case C4_EL_CONTAINER_DB:
        case C4_EL_CONTAINER_QUEUE:
        case C4_EL_CONTAINER_EXT:
        case C4_EL_CONTAINER_EXT_DB:
        case C4_EL_CONTAINER_EXT_QUEUE:
        case C4_EL_COMPONENT:
        case C4_EL_COMPONENT_DB:
        case C4_EL_COMPONENT_QUEUE:
        case C4_EL_COMPONENT_EXT:
        case C4_EL_COMPONENT_EXT_DB:
        case C4_EL_COMPONENT_EXT_QUEUE:
            el->technology = copy_attr(attrs, 2);
            el->descr = copy_attr(attrs, 3);
            el->descr2 = copy_attr(attrs, 4);
            break;
        default:
            // Person, System, Boundary
            el->descr = copy_attr(attrs, 2);
            el->technology = copy_attr(attrs, 3); // Usually sprite or tags for others, but let's stick to simple
            el->descr2 = copy_attr(attrs, 4);
            break;
    }

    // Free attribute list
    while(attrs) {
        C4Attribute* next = attrs->next;
        if(attrs->key) free(attrs->key);
        if(attrs->value) free(attrs->value);
        free(attrs);
        attrs = next;
    }
    return el;
}

void c4_add_element(C4ParserContext* ctx, C4Element* el) {
    el->parent = ctx->current_boundary;
    
    // Add to flat list in diagram for cleanup/traversal, 
    // but maybe also attach to parent's children?
    // The AST defines `children` on C4Element.
    
    if (el->parent) {
        if (!el->parent->children) {
            el->parent->children = el;
        } else {
            C4Element* cur = el->parent->children;
            while(cur->next) cur = cur->next;
            cur->next = el;
        }
    } else {
        // Add to root list
        if (!ctx->diagram->elements) {
            ctx->diagram->elements = el;
        } else {
            C4Element* cur = ctx->diagram->elements;
            while(cur->next) cur = cur->next;
            cur->next = el;
        }
    }
}

C4Rel* c4_create_rel(C4RelType type, C4Attribute* attrs) {
    C4Rel* r = (C4Rel*)calloc(1, sizeof(C4Rel));
    r->type = type;
    
    // Rel(from, to, label, ?tech)
    r->from = copy_attr(attrs, 0);
    r->to = copy_attr(attrs, 1);
    r->label = copy_attr(attrs, 2);
    r->technology = copy_attr(attrs, 3);

    // Free attribute list
    while(attrs) {
        C4Attribute* next = attrs->next;
        if(attrs->key) free(attrs->key);
        if(attrs->value) free(attrs->value);
        free(attrs);
        attrs = next;
    }
    return r;
}

void c4_add_rel(C4ParserContext* ctx, C4Rel* rel) {
    if (!ctx->diagram->relationships) {
        ctx->diagram->relationships = rel;
    } else {
        C4Rel* cur = ctx->diagram->relationships;
        while(cur->next) cur = cur->next;
        cur->next = rel;
    }
}

// --- Parse Function ---

C4Diagram* c4_parse(const char* input) {
    if (!input) return NULL;
    C4ParserContext ctx;
    memset(&ctx, 0, sizeof(C4ParserContext));
    ctx.diagram = (C4Diagram*)calloc(1, sizeof(C4Diagram));
    if (!ctx.diagram) return NULL;
    
    void* parser = C4ParserAlloc(malloc);
    if (!parser) {
        c4_free_diagram(ctx.diagram);
        return NULL;
    }
    
    Scanner s;
    memset(&s, 0, sizeof(Scanner));
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.line = 1;
    s.at_bol = 1;

    c4_scan(&s, parser, &ctx);
    C4Parser(parser, 0, NULL, &ctx); // EOF
    
    C4ParserFree(parser, free);
    if (ctx.error_count > 0) {
        c4_free_diagram(ctx.diagram);
        return NULL;
    }
    return ctx.diagram;
}

static void free_rel(C4Rel* r) {
    while(r) {
        C4Rel* next = r->next;
        if(r->from) free(r->from);
        if(r->to) free(r->to);
        if(r->label) free(r->label);
        if(r->technology) free(r->technology);
        free(r);
        r = next;
    }
}

static void free_elements(C4Element* el) {
    while(el) {
        // Since we link children both in parent->children AND in the main list (if we did that?),
        // we need to be careful about double free. 
        // In c4_add_element implementation above:
        // If parent exists, added to parent->children.
        // If NO parent, added to diagram->elements.
        // Wait, if I add to parent->children, it's NOT in diagram->elements (root list).
        // So I need to recursively free.
        
        C4Element* next = el->next;
        
        if (el->children) {
            free_elements(el->children);
        }
        
        if(el->alias) free(el->alias);
        if(el->label) free(el->label);
        if(el->descr) free(el->descr);
        if(el->technology) free(el->technology);
        if(el->descr2) free(el->descr2);
        if(el->sprite) free(el->sprite);
        if(el->tags) free(el->tags);
        if(el->link) free(el->link);
        free(el);
        
        el = next;
    }
}

void c4_free_diagram(C4Diagram* diagram) {
    if (!diagram) return;
    if (diagram->title) free(diagram->title);
    free_elements(diagram->elements);
    free_rel(diagram->relationships);
    free(diagram);
}

// --- JSON Serialization ---

static const char* element_type_str(C4ElementType type) {
    switch(type) {
        case C4_EL_PERSON: return "Person";
        case C4_EL_PERSON_EXT: return "Person_Ext";
        case C4_EL_SYSTEM: return "System";
        case C4_EL_SYSTEM_DB: return "SystemDb";
        case C4_EL_SYSTEM_QUEUE: return "SystemQueue";
        case C4_EL_SYSTEM_EXT: return "System_Ext";
        case C4_EL_SYSTEM_EXT_DB: return "SystemDb_Ext";
        case C4_EL_SYSTEM_EXT_QUEUE: return "SystemQueue_Ext";
        case C4_EL_CONTAINER: return "Container";
        case C4_EL_CONTAINER_DB: return "ContainerDb";
        case C4_EL_CONTAINER_QUEUE: return "ContainerQueue";
        case C4_EL_CONTAINER_EXT: return "Container_Ext";
        case C4_EL_CONTAINER_EXT_DB: return "ContainerDb_Ext";
        case C4_EL_CONTAINER_EXT_QUEUE: return "ContainerQueue_Ext";
        case C4_EL_COMPONENT: return "Component";
        case C4_EL_COMPONENT_DB: return "ComponentDb";
        case C4_EL_COMPONENT_QUEUE: return "ComponentQueue";
        case C4_EL_COMPONENT_EXT: return "Component_Ext";
        case C4_EL_COMPONENT_EXT_DB: return "ComponentDb_Ext";
        case C4_EL_COMPONENT_EXT_QUEUE: return "ComponentQueue_Ext";
        case C4_EL_BOUNDARY: return "Boundary";
        case C4_EL_ENTERPRISE_BOUNDARY: return "Enterprise_Boundary";
        case C4_EL_SYSTEM_BOUNDARY: return "System_Boundary";
        case C4_EL_CONTAINER_BOUNDARY: return "Container_Boundary";
        case C4_EL_NODE: return "Node";
        case C4_EL_NODE_L: return "Node_L";
        case C4_EL_NODE_R: return "Node_R";
        default: return "Unknown";
    }
}

static const char* diagram_type_str(C4DiagramType type) {
    switch(type) {
        case C4_DT_CONTEXT: return "C4Context";
        case C4_DT_CONTAINER: return "C4Container";
        case C4_DT_COMPONENT: return "C4Component";
        case C4_DT_DYNAMIC: return "C4Dynamic";
        case C4_DT_DEPLOYMENT: return "C4Deployment";
        default: return "Unknown";
    }
}

static const char* rel_type_str(C4RelType type) {
    switch(type) {
        case C4_RT_REL: return "Rel";
        case C4_RT_BIREL: return "BiRel";
        case C4_RT_REL_U: return "Rel_U";
        case C4_RT_REL_D: return "Rel_D";
        case C4_RT_REL_L: return "Rel_L";
        case C4_RT_REL_R: return "Rel_R";
        case C4_RT_REL_B: return "Rel_B";
        default: return "Rel";
    }
}

static json_value_t* serialize_elements(C4Element* el);

static json_value_t* serialize_element(C4Element* el) {
    json_value_t* obj = turbo_json_create_object();
    turbo_json_object_set_string(obj, "type", element_type_str(el->type));
    turbo_json_object_set_string(obj, "alias", el->alias ? el->alias : "");
    turbo_json_object_set_string(obj, "label", el->label ? el->label : "");
    if (el->descr) turbo_json_object_set_string(obj, "descr", el->descr);
    if (el->technology) turbo_json_object_set_string(obj, "technology", el->technology);
    if (el->descr2) turbo_json_object_set_string(obj, "descr2", el->descr2);
    if (el->sprite) turbo_json_object_set_string(obj, "sprite", el->sprite);
    if (el->tags) turbo_json_object_set_string(obj, "tags", el->tags);
    if (el->link) turbo_json_object_set_string(obj, "link", el->link);
    if (el->children) {
        turbo_json_object_add(obj, "children", serialize_elements(el->children));
    }
    return obj;
}

static json_value_t* serialize_elements(C4Element* el) {
    json_value_t* arr = turbo_json_create_array();
    while (el) {
        turbo_json_array_add(arr, serialize_element(el));
        el = el->next;
    }
    return arr;
}

static json_value_t* serialize_relationships(C4Rel* rel) {
    json_value_t* arr = turbo_json_create_array();
    while (rel) {
        json_value_t* obj = turbo_json_create_object();
        turbo_json_object_set_string(obj, "type", rel_type_str(rel->type));
        turbo_json_object_set_string(obj, "from", rel->from ? rel->from : "");
        turbo_json_object_set_string(obj, "to", rel->to ? rel->to : "");
        if (rel->label) turbo_json_object_set_string(obj, "label", rel->label);
        if (rel->technology) turbo_json_object_set_string(obj, "technology", rel->technology);
        turbo_json_array_add(arr, obj);
        rel = rel->next;
    }
    return arr;
}

char* c4_to_json(C4Diagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    turbo_json_object_set_string(root, "type", diagram_type_str(diagram->type));
    if (diagram->title) {
        turbo_json_object_set_string(root, "title", diagram->title);
    }
    turbo_json_object_add(root, "elements", serialize_elements(diagram->elements));
    turbo_json_object_add(root, "relationships", serialize_relationships(diagram->relationships));

    size_t len;
    char* str = turbo_json_serialize_pretty(root, &len);
    turbo_free_json(&root);
    return str;
}
