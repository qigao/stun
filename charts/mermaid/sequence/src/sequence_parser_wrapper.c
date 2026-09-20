#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sequence/sequence_ast.h"
#include "json_parser.h"

// Lexer and Parser functions (generated)
void *SequenceParserAlloc(void *(*mallocProc)(size_t));
void SequenceParser(void *yyp, int yymajor, void* yyminor, SequenceParserContext *ctx);
void SequenceParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void sequence_scan(Scanner *s, void *parser, SequenceParserContext *ctx);

static void free_participants(SequenceParticipant* p) {
    while (p) {
        SequenceParticipant* next = p->next;
        free(p->id);
        free(p->label);
        free(p->type);
        free(p);
        p = next;
    }
}

static void free_statements(SequenceStatement* s) {
    while (s) {
        SequenceStatement* next = s->next;
        if (s->type == SEQ_STMT_SIGNAL) {
            free(s->data.signal.from);
            free(s->data.signal.to);
            free(s->data.signal.message);
        } else if (s->type == SEQ_STMT_NOTE) {
            free(s->data.note.actor);
            free(s->data.note.actor2);
            free(s->data.note.text);
        } else if (s->type == SEQ_STMT_BLOCK) {
            free(s->data.block.text);
            free(s->data.block.alternate_text);
            free_statements(s->data.block.body);
            free_statements(s->data.block.alternate_body);
        } else if (s->type == SEQ_STMT_ACTIVATE || s->type == SEQ_STMT_DEACTIVATE) {
            free(s->data.activation.actor);
        }
        free(s);
        s = next;
    }
}

void sequence_diagram_free(SequenceDiagram* diagram) {
    if (!diagram) return;
    free(diagram->title);
    free_participants(diagram->participants);
    free_statements(diagram->statements);
    free(diagram);
}

SequenceDiagram* sequence_parse(const char* input) {
    if (!input) return NULL;
    const size_t input_size = strlen(input);
    enum { SEQUENCE_SCANNER_PADDING = 16 };
    char* scanner_input =
        (char*)calloc(input_size + SEQUENCE_SCANNER_PADDING, sizeof(char));
    if (!scanner_input) return NULL;
    memcpy(scanner_input, input, input_size);

    SequenceParserContext ctx;
    ctx.diagram = (SequenceDiagram*)malloc(sizeof(SequenceDiagram));
    if (!ctx.diagram) {
        free(scanner_input);
        return NULL;
    }
    memset(ctx.diagram, 0, sizeof(SequenceDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;
    ctx.active_blocks = NULL;

    void* parser = SequenceParserAlloc(malloc);
    if (!parser) {
        free(scanner_input);
        sequence_diagram_free(ctx.diagram);
        return NULL;
    }
    
    Scanner s;
    s.start = scanner_input;
    s.cursor = scanner_input;
    // With YYFILL disabled, re2c requires zeroed lookahead padding at EOF.
    s.limit = scanner_input + input_size + SEQUENCE_SCANNER_PADDING;
    s.marker = NULL;
    s.line = 1;

    sequence_scan(&s, parser, &ctx);
    free(scanner_input);
    
    // Send EOF
    SequenceParser(parser, 0, NULL, &ctx);
    SequenceParserFree(parser, free);

    if (ctx.error_count > 0) {
        sequence_diagram_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

// --- JSON Serialization ---

static const char* signal_type_str(SequenceSignalType type) {
    switch(type) {
        case SEQ_SIGNAL_SOLID: return "solid";
        case SEQ_SIGNAL_DOTTED: return "dotted";
        case SEQ_SIGNAL_SOLID_OPEN: return "solid_open";
        case SEQ_SIGNAL_DOTTED_OPEN: return "dotted_open";
        case SEQ_SIGNAL_SOLID_CROSS: return "solid_cross";
        case SEQ_SIGNAL_DOTTED_CROSS: return "dotted_cross";
        case SEQ_SIGNAL_SOLID_POINT: return "solid_point";
        case SEQ_SIGNAL_DOTTED_POINT: return "dotted_point";
        case SEQ_SIGNAL_BIDIR_SOLID: return "bidir_solid";
        case SEQ_SIGNAL_BIDIR_DOTTED: return "bidir_dotted";
        default: return "solid";
    }
}

static const char* block_type_str(SequenceBlockType type) {
    switch(type) {
        case SEQ_BLOCK_LOOP: return "loop";
        case SEQ_BLOCK_ALT: return "alt";
        case SEQ_BLOCK_OPT: return "opt";
        case SEQ_BLOCK_PAR: return "par";
        case SEQ_BLOCK_RECT: return "rect";
        case SEQ_BLOCK_CRITICAL: return "critical";
        case SEQ_BLOCK_BREAK: return "break";
        default: return "loop";
    }
}

static const char* note_placement_str(SequenceNotePlacement placement) {
    switch(placement) {
        case SEQ_NOTE_LEFT_OF: return "left_of";
        case SEQ_NOTE_RIGHT_OF: return "right_of";
        case SEQ_NOTE_OVER: return "over";
        default: return "right_of";
    }
}

static json_value_t* serialize_participants(SequenceParticipant* p) {
    json_value_t* arr = json_create_array();
    while (p) {
        json_value_t* obj = json_create_object();
        json_object_set_string(obj, "id", p->id ? p->id : "");
        if (p->label && strcmp(p->label, p->id) != 0) {
            json_object_set_string(obj, "label", p->label);
        }
        if (p->type) {
            json_object_set_string(obj, "type", p->type);
        }
        json_array_add(arr, obj);
        p = p->next;
    }
    return arr;
}

static json_value_t* serialize_statements(SequenceStatement* s);

static json_value_t* serialize_statement(SequenceStatement* s) {
    json_value_t* obj = json_create_object();

    switch(s->type) {
        case SEQ_STMT_SIGNAL:
            json_object_set_string(obj, "type", "signal");
            json_object_set_string(obj, "from", s->data.signal.from ? s->data.signal.from : "");
            json_object_set_string(obj, "to", s->data.signal.to ? s->data.signal.to : "");
            if (s->data.signal.message) json_object_set_string(obj, "message", s->data.signal.message);
            json_object_set_string(obj, "signalType", signal_type_str(s->data.signal.signal_type));
            if (s->data.signal.activate) json_object_set_bool(obj, "activate", 1);
            if (s->data.signal.deactivate) json_object_set_bool(obj, "deactivate", 1);
            break;
        case SEQ_STMT_NOTE:
            json_object_set_string(obj, "type", "note");
            json_object_set_string(obj, "actor", s->data.note.actor ? s->data.note.actor : "");
            if (s->data.note.actor2) json_object_set_string(obj, "actor2", s->data.note.actor2);
            if (s->data.note.text) json_object_set_string(obj, "text", s->data.note.text);
            json_object_set_string(obj, "placement", note_placement_str(s->data.note.placement));
            break;
        case SEQ_STMT_BLOCK:
            json_object_set_string(obj, "type", "block");
            json_object_set_string(obj, "blockType", block_type_str(s->data.block.type));
            if (s->data.block.text) json_object_set_string(obj, "text", s->data.block.text);
            if (s->data.block.body) {
                json_object_add(obj, "body", serialize_statements(s->data.block.body));
            }
            if (s->data.block.alternate_body) {
                json_object_add(obj, "alternateBody", serialize_statements(s->data.block.alternate_body));
                if (s->data.block.alternate_text) {
                    json_object_set_string(obj, "alternateText", s->data.block.alternate_text);
                }
            }
            break;
        case SEQ_STMT_ACTIVATE:
            json_object_set_string(obj, "type", "activate");
            json_object_set_string(obj, "actor", s->data.activation.actor ? s->data.activation.actor : "");
            break;
        case SEQ_STMT_DEACTIVATE:
            json_object_set_string(obj, "type", "deactivate");
            json_object_set_string(obj, "actor", s->data.activation.actor ? s->data.activation.actor : "");
            break;
        case SEQ_STMT_AUTONUMBER:
            json_object_set_string(obj, "type", "autonumber");
            json_object_set_number(obj, "start", s->data.autonumber.start);
            json_object_set_number(obj, "step", s->data.autonumber.step);
            json_object_set_bool(obj, "visible", s->data.autonumber.visible);
            break;
    }
    return obj;
}

static json_value_t* serialize_statements(SequenceStatement* s) {
    json_value_t* arr = json_create_array();
    while (s) {
        json_array_add(arr, serialize_statement(s));
        s = s->next;
    }
    return arr;
}

char* sequence_to_json(SequenceDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = json_create_object();
    json_object_set_string(root, "type", "sequenceDiagram");

    if (diagram->title) {
        json_object_set_string(root, "title", diagram->title);
    }

    json_object_add(root, "participants", serialize_participants(diagram->participants));
    json_object_add(root, "statements", serialize_statements(diagram->statements));

    size_t len;
    char* str = json_serialize_pretty(root, &len);
    json_free(&root);
    return str;
}
