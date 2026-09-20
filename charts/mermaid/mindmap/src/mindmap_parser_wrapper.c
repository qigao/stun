#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mindmap/mindmap_ast.h"
#include "mindmap_parser_gen.h"
#include "json_parser.h"

void *MindmapParserAlloc(void *(*mallocProc)(size_t));
void MindmapParser(void *yyp, int yymajor, void* yyminor, MindmapParserContext *ctx);
void MindmapParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
    int at_bol;
} Scanner;

void mindmap_scan(Scanner *s, void *parser, MindmapParserContext *ctx);

static void free_node(MindmapNode* node) {
    if (!node) return;
    if (node->children) free_node(node->children);
    if (node->next) free_node(node->next);
    free(node->id);
    free(node->label);
    free(node->icon);
    free(node->classes);
    free(node);
}

void mindmap_free(MindmapDiagram* diagram) {
    if (!diagram) return;
    free_node(diagram->root);
    free(diagram);
}

// Takes ownership of id and label strings
MindmapNode* mindmap_create_node(const char* id, const char* label, MindmapNodeType type, int level) {
    MindmapNode* node = (MindmapNode*)malloc(sizeof(MindmapNode));
    memset(node, 0, sizeof(MindmapNode));
    
    // Safety check for nulls
    char* id_ptr = (char*)id;
    char* label_ptr = (char*)label;

    node->id = id_ptr;
    if (id_ptr == label_ptr && id_ptr != NULL) {
        node->label = strdup(label_ptr);
    } else {
        node->label = label_ptr;
    }
    
    node->type = type;
    node->level = level;
    return node;
}

void mindmap_add_node(MindmapParserContext* ctx, MindmapNode* node) {
    if (!ctx->diagram->root) {
        ctx->diagram->root = node;
        ctx->diagram->last_node = node;
        node->depth = 0;
        return;
    }

    MindmapNode* parent = ctx->diagram->last_node;
    if (parent && node->level > parent->level) {
        // Child
        if (!parent->children) {
            parent->children = node;
        } else {
            MindmapNode* last_child = parent->children;
            while(last_child->next) last_child = last_child->next;
            last_child->next = node;
        }
        node->parent = parent;
        node->depth = parent->depth + 1;
    } else {
        // Find ancestor
        while (parent && parent->level >= node->level) {
            parent = parent->parent;
        }
        
        if (parent) {
            // Sibling of parent's child
            if (!parent->children) {
                 parent->children = node;
            } else {
                MindmapNode* last_child = parent->children;
                while(last_child->next) last_child = last_child->next;
                last_child->next = node;
            }
            node->parent = parent;
            node->depth = parent->depth + 1;
        } else {
            // Sibling of root
            MindmapNode* last_root = ctx->diagram->root;
            while(last_root->next) last_root = last_root->next;
            last_root->next = node;
            node->parent = NULL;
            node->depth = 0; // Roots are depth 0
        }
    }
    
    ctx->diagram->last_node = node;
}

void mindmap_add_icon(MindmapParserContext* ctx, const char* icon) {
    if (ctx->diagram->last_node) {
        if (ctx->diagram->last_node->icon) free(ctx->diagram->last_node->icon);
        ctx->diagram->last_node->icon = (char*)icon; // Take ownership
    } else {
        free((char*)icon);
    }
}

void mindmap_add_class(MindmapParserContext* ctx, const char* class_name) {
     if (ctx->diagram->last_node) {
        if (ctx->diagram->last_node->classes) {
            size_t len = strlen(ctx->diagram->last_node->classes) + strlen(class_name) + 2;
            char* new_classes = (char*)malloc(len);
            sprintf(new_classes, "%s %s", ctx->diagram->last_node->classes, class_name);
            free(ctx->diagram->last_node->classes);
            free((char*)class_name); // Free the input arg since we combined it
            ctx->diagram->last_node->classes = new_classes;
        } else {
            ctx->diagram->last_node->classes = (char*)class_name; // Take ownership
        }
    } else {
        free((char*)class_name);
    }
}

static const char* node_type_to_shape(MindmapNodeType type) {
    switch(type) {
        case MINDMAP_NODE_DEFAULT: return "default";
        case MINDMAP_NODE_SQUARE: return "square";
        case MINDMAP_NODE_ROUNDED: return "rounded";
        case MINDMAP_NODE_CIRCLE: return "circle";
        case MINDMAP_NODE_CLOUD: return "cloud";
        case MINDMAP_NODE_BANG: return "bang";
        case MINDMAP_NODE_HEXAGON: return "hexagon";
        default: return "default";
    }
}

static json_value_t* mindmap_node_to_json_recursive(MindmapNode* node) {
    if(!node) return NULL;
    
    json_value_t* obj = json_create_object();
    
    // Add children first (alphabetically first)
    json_value_t* children_arr = json_create_array();
    MindmapNode* child = node->children;
    while(child) {
        json_value_t* child_obj = mindmap_node_to_json_recursive(child);
        if(child_obj) {
            json_array_add(children_arr, child_obj);
        }
        child = child->next;
    }
    json_object_add(obj, "children", children_arr);
    
    // Then add other keys in alphabetical order
    json_object_set_string(obj, "description", node->label ? node->label : "");
    json_object_set_string(obj, "id", node->id ? node->id : "");
    json_object_set_number(obj, "level", (double)node->depth);
    json_object_set_string(obj, "shape", node_type_to_shape(node->type));
    
    return obj;
}

char* mindmap_to_json(MindmapDiagram* d) {
    if(!d) {
        json_value_t *null_val = json_create_null();
        size_t len;
        char *s = json_serialize_pretty_crlf(null_val, &len);
        json_free(&null_val);
        return s;
    }
    
    json_value_t *root = json_create_object();
    
    // Add keys in alphabetical order: root, type
    if(d->root) {
        json_value_t* root_node = mindmap_node_to_json_recursive(d->root);
        json_object_add(root, "root", root_node); 
    }
    
    json_object_set_string(root, "type", "mindmap");
    
    size_t len;
    char *s = json_serialize_pretty_crlf(root, &len);
    json_free(&root);
    return s;
}

MindmapDiagram* mindmap_parse(const char* input) {
    if (!input) return NULL;
    MindmapParserContext ctx;
    ctx.diagram = (MindmapDiagram*)malloc(sizeof(MindmapDiagram));
    if (!ctx.diagram) return NULL;
    memset(ctx.diagram, 0, sizeof(MindmapDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;

    void* parser = MindmapParserAlloc(malloc);
    if (!parser) {
        mindmap_free(ctx.diagram);
        return NULL;
    }
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;
    s.at_bol = 1;

    mindmap_scan(&s, parser, &ctx);
    
    MindmapParser(parser, MINDMAP_NL, NULL, &ctx);
    MindmapParser(parser, 0, NULL, &ctx);
    MindmapParserFree(parser, free);

    if (ctx.error_count > 0) {
        mindmap_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}
