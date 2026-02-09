#ifndef ER_AST_H
#define ER_AST_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ER_CARDINALITY_ZERO_OR_ONE,
    ER_CARDINALITY_ZERO_OR_MORE,
    ER_CARDINALITY_ONE_OR_MORE,
    ER_CARDINALITY_ONLY_ONE
} ERCardinality;

typedef enum {
    ER_REL_NON_IDENTIFYING, // ..
    ER_REL_IDENTIFYING      // --
} ERRelType;

typedef struct ERAttribute {
    char* type;
    char* name;
    char* keys; // PK, FK, UK (comma separated or handled differently?)
    char* comment;
    struct ERAttribute* next;
} ERAttribute;

typedef struct EREntity {
    char* name;
    ERAttribute* attributes;
    struct EREntity* next;
} EREntity;

typedef struct ERRelationship {
    char* entity1;
    char* entity2;
    char* role; // Label
    ERCardinality card1;
    ERCardinality card2;
    ERRelType type;
    struct ERRelationship* next;
} ERRelationship;

typedef struct ERDiagram {
    char* title;
    EREntity* entities;
    ERRelationship* relationships;
} ERDiagram;

typedef struct {
    ERDiagram* diagram;
    int error_count;
    char* error_message;
} ERParserContext;

#ifdef __cplusplus
}
#endif

#endif // ER_AST_H
