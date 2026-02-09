#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "er/er_ast.h"
#include "er_parser_gen.h"
#include "turbo_parser.h"

void *ERParserAlloc(void *(*mallocProc)(size_t));
void ERParser(void *yyp, int yymajor, void* yyminor, ERParserContext *ctx);
void ERParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void er_scan(Scanner *s, void *parser, ERParserContext *ctx);

static void free_entity(EREntity* e) {
    while (e) {
        EREntity* next = e->next;
        free(e->name);
        ERAttribute* a = e->attributes;
        while(a) {
            ERAttribute* anext = a->next;
            free(a->name);
            free(a->type);
            free(a->keys);
            free(a->comment);
            free(a);
            a = anext;
        }
        free(e);
        e = next;
    }
}

static void free_relationships(ERRelationship* r) {
    while (r) {
        ERRelationship* next = r->next;
        free(r->entity1);
        free(r->entity2);
        free(r->role);
        free(r);
        r = next;
    }
}

void er_free(ERDiagram* diagram) {
    if (!diagram) return;
    free(diagram->title);
    free_entity(diagram->entities);
    free_relationships(diagram->relationships);
    free(diagram);
}

ERDiagram* er_parse(const char* input) {
    ERParserContext ctx;
    ctx.diagram = (ERDiagram*)malloc(sizeof(ERDiagram));
    memset(ctx.diagram, 0, sizeof(ERDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = ERParserAlloc(malloc);
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    er_scan(&s, parser, &ctx);
    
    // Explicitly signal end of input to Lemon parser
    ERParser(parser, 0, NULL, &ctx);
    
    ERParserFree(parser, free);

    if (ctx.error_count > 0) {
        er_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

static const char* map_cardinality(ERCardinality c) {
    switch(c) {
        case ER_CARDINALITY_ZERO_OR_ONE: return "ZERO_OR_ONE";
        case ER_CARDINALITY_ZERO_OR_MORE: return "ZERO_OR_MORE";
        case ER_CARDINALITY_ONE_OR_MORE: return "ONE_OR_MORE";
        case ER_CARDINALITY_ONLY_ONE: return "ONLY_ONE";
        default: return "UNKNOWN";
    }
}

static const char* map_rel_type(ERRelType t) {
    switch(t) {
        case ER_REL_NON_IDENTIFYING: return "NON_IDENTIFYING";
        case ER_REL_IDENTIFYING: return "IDENTIFYING";
        default: return "UNKNOWN";
    }
}

char* er_to_json(ERDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    turbo_json_object_set_string(root, "direction", "TB"); // Default
    turbo_json_object_set_string(root, "type", "erDiagram");
    turbo_json_object_add(root, "classDefs", turbo_json_create_object());
    turbo_json_object_add(root, "classes", turbo_json_create_object());
    turbo_json_object_add(root, "styles", turbo_json_create_object());

    // Entities
    json_value_t* entities_obj = turbo_json_create_object();
    EREntity* e = diagram->entities;
    while(e) {
        if (e->name) {
            json_value_t* e_obj = turbo_json_create_object();
            turbo_json_object_set_string(e_obj, "name", e->name);
            
            json_value_t* attrs_arr = turbo_json_create_array();
            ERAttribute* a = e->attributes;
            while(a) {
                json_value_t* a_obj = turbo_json_create_object();
                if (a->name) turbo_json_object_set_string(a_obj, "name", a->name);
                if (a->type) turbo_json_object_set_string(a_obj, "type", a->type);
                if (a->keys) turbo_json_object_set_string(a_obj, "keys", a->keys);
                if (a->comment) turbo_json_object_set_string(a_obj, "comment", a->comment);
                turbo_json_array_add(attrs_arr, a_obj);
                a = a->next;
            }
            turbo_json_object_add(e_obj, "attributes", attrs_arr);
            turbo_json_object_add(entities_obj, e->name, e_obj);
            // Hint: In render-basic.json, entities is a map, keyed by name.
        }
        e = e->next;
    }
    turbo_json_object_add(root, "entities", entities_obj);

    // Relationships
    json_value_t* rels_arr = turbo_json_create_array();
    ERRelationship* r = diagram->relationships;
    while(r) {
        json_value_t* r_obj = turbo_json_create_object();
        turbo_json_object_set_string(r_obj, "entityA", r->entity1 ? r->entity1 : "");
        turbo_json_object_set_string(r_obj, "entityB", r->entity2 ? r->entity2 : "");
        turbo_json_object_set_string(r_obj, "role", r->role ? r->role : "");
        
        json_value_t* spec = turbo_json_create_object();
        turbo_json_object_set_string(spec, "cardA", map_cardinality(r->card1));
        turbo_json_object_set_string(spec, "cardB", map_cardinality(r->card2));
        turbo_json_object_set_string(spec, "relType", map_rel_type(r->type));
        turbo_json_object_add(r_obj, "relSpec", spec);

        turbo_json_array_add(rels_arr, r_obj);
        r = r->next;
    }
    turbo_json_object_add(root, "relationships", rels_arr);

    size_t len;
    char* str = turbo_json_serialize_pretty(root, &len);
    turbo_free_json(&root);
    return str;
}
