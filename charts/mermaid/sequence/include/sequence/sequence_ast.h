#ifndef SEQUENCE_AST_H
#define SEQUENCE_AST_H

#include <stdlib.h>

typedef enum {
    SEQ_SIGNAL_SOLID,
    SEQ_SIGNAL_DOTTED,
    SEQ_SIGNAL_SOLID_OPEN,
    SEQ_SIGNAL_DOTTED_OPEN,
    SEQ_SIGNAL_SOLID_CROSS,
    SEQ_SIGNAL_DOTTED_CROSS,
    SEQ_SIGNAL_SOLID_POINT,
    SEQ_SIGNAL_DOTTED_POINT,
    SEQ_SIGNAL_BIDIR_SOLID,
    SEQ_SIGNAL_BIDIR_DOTTED
} SequenceSignalType;

typedef enum {
    SEQ_BLOCK_LOOP,
    SEQ_BLOCK_ALT,
    SEQ_BLOCK_OPT,
    SEQ_BLOCK_PAR,
    SEQ_BLOCK_RECT,
    SEQ_BLOCK_CRITICAL,
    SEQ_BLOCK_BREAK
} SequenceBlockType;

typedef enum {
    SEQ_NOTE_LEFT_OF,
    SEQ_NOTE_RIGHT_OF,
    SEQ_NOTE_OVER
} SequenceNotePlacement;

typedef struct SequenceBlock {
    char* alternate_text;
    struct SequenceStatement* alternate_body;
} SequenceBlock;

typedef struct SequenceParticipant {
    char* id;
    char* label;
    char* type; // "actor" or "participant"
    struct SequenceParticipant* next;
} SequenceParticipant;

typedef struct SequenceStatement {
    enum {
        SEQ_STMT_SIGNAL,
        SEQ_STMT_NOTE,
        SEQ_STMT_BLOCK,
        SEQ_STMT_ACTIVATE,
        SEQ_STMT_DEACTIVATE,
        SEQ_STMT_AUTONUMBER
    } type;
    
    union {
        struct {
            char* from;
            char* to;
            char* message;
            SequenceSignalType signal_type;
            int activate;
            int deactivate;
        } signal;
        
        struct {
            char* actor;
            char* actor2; // for "over A, B"
            char* text;
            SequenceNotePlacement placement;
        } note;
        
        struct {
            SequenceBlockType type;
            char* text;
            struct SequenceStatement* body;
            struct SequenceStatement* alternate_body; // for "else"
            char* alternate_text;
        } block;
        
        struct {
            char* actor;
        } activation;
        
        struct {
            int start;
            int step;
            int visible;
        } autonumber;
    } data;
    
    struct SequenceStatement* next;
} SequenceStatement;

typedef struct SequenceDiagram {
    char* title;
    SequenceParticipant* participants;
    SequenceStatement* statements;
} SequenceDiagram;

typedef struct {
    SequenceDiagram* diagram;
    int error_count;
    char* error_message;
    void* active_blocks; // Stack for nested blocks
} SequenceParserContext;

#endif // SEQUENCE_AST_H
