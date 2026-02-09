#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kanban/kanban_ast.h"

static char* copy_string(const char* s) {
    if(!s) return NULL;
    return strdup(s);
}

KanbanDiagram* kanban_create_diagram() {
    KanbanDiagram* d = (KanbanDiagram*)malloc(sizeof(KanbanDiagram));
    if(d) {
        memset(d, 0, sizeof(KanbanDiagram));
        // Create a root node for the diagram top level
        d->root = kanban_create_node("root", "root", KANBAN_NODE_DEFAULT, -1);
    }
    return d;
}

static void free_node_rewrite(KanbanNode* n) {
    if(!n) return;
    free_node_rewrite(n->children);
    free_node_rewrite(n->next);
    free(n->id);
    free(n->label);
    free(n->icon);
    free(n->classes);
    free(n->shape_data);
    free(n);
}

void kanban_free_diagram(KanbanDiagram* d) {
    if(!d) return;
    free_node_rewrite(d->root);
    free(d);
}

KanbanNode* kanban_create_node(const char* id, const char* label, KanbanNodeShape type, int level) {
    KanbanNode* n = (KanbanNode*)malloc(sizeof(KanbanNode));
    if(n) {
        memset(n, 0, sizeof(KanbanNode));
        n->id = copy_string(id);
        n->label = copy_string(label);
        n->type = type;
        n->level = level;
    }
    return n;
}

void kanban_add_node(KanbanParserContext* ctx, KanbanNode* node) {
    if(!ctx || !ctx->diagram) return;
    
    // Determine parent
    KanbanNode* parent = ctx->diagram->root; // Default parent
    
    // Look for a node with level < node->level in the stack
    for(int i = node->level - 1; i >= 0; i--) {
        if(ctx->last_node_at_level[i]) {
            parent = ctx->last_node_at_level[i];
            break;
        }
    }
    
    // Calculate depth based on parent
    if(parent == ctx->diagram->root) {
        node->depth = 0; // Direct children of root are depth 0
    } else {
        node->depth = parent->depth + 1;
    }
    
    // Add to parent
    if(!parent->children) {
        parent->children = node;
    } else {
        KanbanNode* sibling = parent->children;
        while(sibling->next) {
            sibling = sibling->next;
        }
        sibling->next = node;
    }
    
    // Update state
    if(node->level >= 0 && node->level < 64) {
        ctx->last_node_at_level[node->level] = node;
        // Invalidate deeper levels?
        for(int k = node->level + 1; k < 64; k++) {
            ctx->last_node_at_level[k] = NULL;
        }
    }
    ctx->last_added_node = node;
}

void kanban_add_icon(KanbanParserContext* ctx, const char* icon) {
    if(ctx && ctx->last_added_node) {
        // Append or replace? usually replace or list. 
        // AST has char* icon. Replace.
        if(ctx->last_added_node->icon) free(ctx->last_added_node->icon);
        ctx->last_added_node->icon = copy_string(icon);
    }
}

void kanban_add_class(KanbanParserContext* ctx, const char* class_name) {
    if(ctx && ctx->last_added_node) {
         if(ctx->last_added_node->classes) free(ctx->last_added_node->classes);
         ctx->last_added_node->classes = copy_string(class_name);
    }
}

void kanban_add_shape_data(KanbanParserContext* ctx, const char* data) {
    if(ctx && ctx->last_added_node) {
         if(ctx->last_added_node->shape_data) free(ctx->last_added_node->shape_data);
         ctx->last_added_node->shape_data = copy_string(data);
    }
}
