#ifndef C4_AST_H
#define C4_AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    C4_DT_CONTEXT,
    C4_DT_CONTAINER,
    C4_DT_COMPONENT,
    C4_DT_DYNAMIC,
    C4_DT_DEPLOYMENT
} C4DiagramType;

typedef enum {
    C4_EL_PERSON, C4_EL_PERSON_EXT,
    C4_EL_SYSTEM, C4_EL_SYSTEM_DB, C4_EL_SYSTEM_QUEUE, C4_EL_SYSTEM_EXT, C4_EL_SYSTEM_EXT_DB, C4_EL_SYSTEM_EXT_QUEUE,
    C4_EL_CONTAINER, C4_EL_CONTAINER_DB, C4_EL_CONTAINER_QUEUE, C4_EL_CONTAINER_EXT, C4_EL_CONTAINER_EXT_DB, C4_EL_CONTAINER_EXT_QUEUE,
    C4_EL_COMPONENT, C4_EL_COMPONENT_DB, C4_EL_COMPONENT_QUEUE, C4_EL_COMPONENT_EXT, C4_EL_COMPONENT_EXT_DB, C4_EL_COMPONENT_EXT_QUEUE,
    C4_EL_BOUNDARY, C4_EL_ENTERPRISE_BOUNDARY, C4_EL_SYSTEM_BOUNDARY, C4_EL_CONTAINER_BOUNDARY,
    C4_EL_NODE, C4_EL_NODE_L, C4_EL_NODE_R
} C4ElementType;

typedef struct C4Element {
    C4ElementType type;
    char* alias;
    char* label;
    char* descr; // Or type/technology ?
    char* technology;
    char* descr2;
    char* sprite;
    char* tags;
    char* link;
    struct C4Element* parent; // For boundaries
    struct C4Element* children; // For boundaries
    struct C4Element* next;
} C4Element;

typedef enum {
    C4_RT_REL, C4_RT_BIREL, 
    C4_RT_REL_U, C4_RT_REL_D, C4_RT_REL_L, C4_RT_REL_R, C4_RT_REL_B
} C4RelType;

typedef struct C4Rel {
    C4RelType type;
    char* from;
    char* to;
    char* label;
    char* technology;
    struct C4Rel* next;
} C4Rel;

typedef struct C4Attribute {
    char* value;
    char* key; // Optional
    struct C4Attribute* next;
} C4Attribute;

typedef struct {
    C4DiagramType type;
    char* title;
    C4Element* elements;
    C4Rel* relationships;
} C4Diagram;

typedef struct {
    C4Diagram* diagram;
    C4Element* current_boundary; // Stack or ptr?
    int error_count;
    char* error_message;
} C4ParserContext;

// Helper functions
void c4_add_element(C4ParserContext* ctx, C4Element* element);
void c4_add_rel(C4ParserContext* ctx, C4Rel* rel);
// void c4_set_title(C4ParserContext* ctx, const char* title);

#ifdef __cplusplus
}
#endif

#endif // C4_AST_H
