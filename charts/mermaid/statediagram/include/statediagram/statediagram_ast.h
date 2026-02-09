#ifndef STATEDIAGRAM_AST_H
#define STATEDIAGRAM_AST_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_TYPE_DEFAULT,
    STATE_TYPE_START_END, // [*]
    STATE_TYPE_COMPOSITE, // Group
    STATE_TYPE_FORK,      // <<fork>>
    STATE_TYPE_JOIN,      // <<join>>
    STATE_TYPE_CHOICE,    // <<choice>>
    STATE_TYPE_DIVIDER,   // -- (concurrent)
    STATE_TYPE_NOTE       // note attached or floating
} StateNodeType;

typedef struct StateDoc StateDoc;

typedef struct StateNode {
    StateNodeType type;
    char* id;
    char* description;
    StateDoc* doc; // For composite states
    struct StateNode* next;
} StateNode;

typedef struct StateTransition {
    char* id1;
    char* id2;
    char* description;
    struct StateTransition* next;
} StateTransition;

typedef struct StateNote {
    char* id; // Associated state if any
    char* text; 
    char* position; // left of, right of
    struct StateNote* next;
} StateNote;

struct StateDoc {
    StateNode* nodes;
    StateTransition* transitions;
    StateNote* notes;
    struct StateDoc* next; // Just for convenience list if needed, usually single doc per node
};

typedef struct StateDiagram {
    char* title;
    char* direction; // TB, LR, etc.
    StateDoc* root;
} StateDiagram;

typedef struct {
    StateDiagram* diagram;
    StateDoc* current_doc; // To handle nesting context
    // Stack for nested docs if needed, or simply pass down
    void* doc_stack; 
    int error_count;
    char* error_message;
} StateParserContext;

#ifdef __cplusplus
}
#endif

#endif // STATEDIAGRAM_AST_H
