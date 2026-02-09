#ifndef BLOCK_AST_H
#define BLOCK_AST_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLOCK_STMT_NODE,
    BLOCK_STMT_EDGE,
    BLOCK_STMT_COLUMNS,
    BLOCK_STMT_SPACE,
    BLOCK_STMT_CLASSDEF,
    BLOCK_STMT_APPLYCLASS,
    BLOCK_STMT_STYLE
} BlockStmtType;

typedef struct BlockStatement BlockStatement;

typedef struct {
    char* id;
    char* label;
    char* shape; // e.g. "rect", "round", etc. derived from braces
    int width;
    struct BlockStatement* children; // For composite blocks
    char** directions; // For arrow blocks
    int direction_count;
} BlockNode;

typedef struct {
    char* id1;
    char* id2;
    char* label;
    char* edgeType; // --, ->, etc.
} BlockEdge;

typedef struct {
    int count; // -1 for auto
} BlockColumns;

typedef struct {
    int width;
} BlockSpace;

typedef struct {
    char* id;
    char* styles;
} BlockClassDef;

typedef struct {
    char* id;
    char* className;
} BlockApplyClass;

typedef struct {
    char* id;
    char* styles;
} BlockStyle;

struct BlockStatement {
    BlockStmtType type;
    union {
        BlockNode node;
        BlockEdge edge;
        BlockColumns columns;
        BlockSpace space;
        BlockClassDef classDef;
        BlockApplyClass applyClass;
        BlockStyle style;
    } data;
    struct BlockStatement* next;
};

typedef struct {
    char* hierarchy; // block-beta or block
    BlockStatement* statements;
} BlockDiagram;

typedef struct {
    BlockDiagram* diagram;
    BlockStatement* current_container; // For nested blocks
    void* container_stack;
    int error_count;
    char* error_message;
} BlockParserContext;

#ifdef __cplusplus
}
#endif

#endif // BLOCK_AST_H
