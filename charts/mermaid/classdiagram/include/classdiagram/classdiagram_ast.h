#ifndef CLASSDIAGRAM_AST_H
#define CLASSDIAGRAM_AST_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLASS_VISIBILITY_NONE,
    CLASS_VISIBILITY_PUBLIC,    // +
    CLASS_VISIBILITY_PRIVATE,   // -
    CLASS_VISIBILITY_PROTECTED, // #
    CLASS_VISIBILITY_INTERNAL   // ~
} ClassVisibility;

typedef enum {
    CLASS_REL_INHERITANCE,      // --|>
    CLASS_REL_COMPOSITION,      // --*
    CLASS_REL_AGGREGATION,      // --o
    CLASS_REL_ASSOCIATION,      // -->
    CLASS_REL_DEPENDENCY,       // ..>
    CLASS_REL_REALIZATION,      // ..|>
    CLASS_REL_LINK              // --
} ClassRelationshipType;

typedef struct ClassMember {
    char* name;
    char* type;
    char* return_type;
    ClassVisibility visibility;
    int is_static;
    int is_abstract;
    int is_method;
    struct ClassMember* next;
} ClassMember;

typedef struct ClassNode {
    char* name;
    char* annotation; // <<interface>>, <<enumeration>>, etc.
    ClassMember* members;
    struct ClassNode* next;
} ClassNode;

typedef struct ClassRelationship {
    char* from;
    char* to;
    char* label;
    char* from_cardinality;
    char* to_cardinality;
    ClassRelationshipType type;
    int is_dotted;
    struct ClassRelationship* next;
} ClassRelationship;

typedef struct ClassDiagram {
    char* title;
    ClassNode* classes;
    ClassRelationship* relationships;
} ClassDiagram;

typedef struct {
    ClassDiagram* diagram;
    int error_count;
    char* error_message;
    ClassNode* current_class;
    char* last_id;
} ClassParserContext;

#ifdef __cplusplus
}
#endif

#endif // CLASSDIAGRAM_AST_H
