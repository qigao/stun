#ifndef KANBAN_AST_H
#define KANBAN_AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KANBAN_NODE_DEFAULT,
    KANBAN_NODE_SQUARE,
    KANBAN_NODE_ROUNDED,
    KANBAN_NODE_CIRCLE,
    KANBAN_NODE_CLOUD,
    KANBAN_NODE_BANG,
    KANBAN_NODE_HEXAGON
} KanbanNodeShape;

typedef struct KanbanNode {
    char* id;
    char* label;
    KanbanNodeShape type;
    int level;          // Raw indentation level
    int depth;          // Semantic tree depth (0, 1, 2...)
    char* icon;
    char* classes;
    char* shape_data;
    
    struct KanbanNode* children;
    struct KanbanNode* next;
    
    // Parent pointer for stack traversal during construction?
    // Or we keep a stack in context.
} KanbanNode;

typedef struct KanbanDiagram {
    KanbanNode* root;
    // Helper to keep track of last node at each level for building tree
} KanbanDiagram;

typedef struct {
    KanbanDiagram* diagram;
    int error_count;
    char* error_message;
    
    // For tree construction
    struct KanbanNode* last_node_at_level[64]; // Map indentation level to node
    struct KanbanNode* last_added_node;
} KanbanParserContext;

KanbanDiagram* kanban_create_diagram();
void kanban_free_diagram(KanbanDiagram* d);

KanbanNode* kanban_create_node(const char* id, const char* label, KanbanNodeShape type, int level);
void kanban_add_node(KanbanParserContext* ctx, KanbanNode* node);
void kanban_add_icon(KanbanParserContext* ctx, const char* icon);
void kanban_add_class(KanbanParserContext* ctx, const char* class_name);
void kanban_add_shape_data(KanbanParserContext* ctx, const char* data);

#ifdef __cplusplus
}
#endif

#endif // KANBAN_AST_H
