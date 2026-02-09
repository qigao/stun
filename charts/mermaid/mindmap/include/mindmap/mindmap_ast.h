#ifndef MINDMAP_AST_H
#define MINDMAP_AST_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MINDMAP_NODE_DEFAULT,
    MINDMAP_NODE_SQUARE,      // [ ]
    MINDMAP_NODE_ROUNDED,     // ( )
    MINDMAP_NODE_CIRCLE,      // (( ))
    MINDMAP_NODE_CLOUD,       // ) (
    MINDMAP_NODE_BANG,        // )) ((
    MINDMAP_NODE_HEXAGON      // {{ }}
} MindmapNodeType;

typedef struct MindmapNode {
    char* id;
    char* label;
    MindmapNodeType type;
    char* icon;         // e.g. ::icon(fa fa-book)
    char* classes;      // e.g. :::class1

    int level;          // indentation level
    int depth;          // tree depth
    struct MindmapNode* parent;
    struct MindmapNode* children; // First child
    struct MindmapNode* next;     // Next sibling
} MindmapNode;

typedef struct {
    MindmapNode* root;
    MindmapNode* last_node; // To facilitate tree construction based on indentation
} MindmapDiagram;

typedef struct {
    MindmapDiagram* diagram;
    int error_count;
    char* error_message;
} MindmapParserContext;

// Helper function declarations
MindmapNode* mindmap_create_node(const char* id, const char* label, MindmapNodeType type, int level);
void mindmap_add_node(MindmapParserContext* ctx, MindmapNode* node);
void mindmap_add_icon(MindmapParserContext* ctx, const char* icon);
void mindmap_add_class(MindmapParserContext* ctx, const char* class_name);

char* mindmap_to_json(MindmapDiagram* diagram);

#ifdef __cplusplus
}
#endif

#endif // MINDMAP_AST_H
