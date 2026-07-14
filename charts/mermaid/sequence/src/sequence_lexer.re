#include <stdlib.h>
#include <string.h>
#include "sequence/sequence_ast.h"
#include "sequence_parser_gen.h"

void SequenceParser(void *parser, int token, void *value,
                    SequenceParserContext *ctx);

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

static char* copy_token(const char* start, const char* end) {
    size_t len = end - start;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    return res;
}

static char* trim_label(const char* start, const char* end) {
    // Skip leading colon and spaces
    while (start < end && (*start == ':' || *start == ' ' || *start == '\t')) start++;
    // Skip trailing spaces
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    return copy_token(start, end);
}

void sequence_scan(Scanner *s, void *parser, SequenceParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        white = [ \t\r]+;
        newline = [\n];
        comment = ("%%"| "#") [^\n\x00]* newline;
        ident = [a-zA-Z0-9_]+([a-zA-Z0-9_]* "-" [a-zA-Z0-9_]+)*;
        string = "\"" [^"\x00]* "\"";

        white { goto loop; }
        newline { s->line++; SequenceParser(parser, SEQ_NEWLINE, NULL, ctx); goto loop; }
        comment { s->line++; SequenceParser(parser, SEQ_NEWLINE, NULL, ctx); goto loop; }

        "sequenceDiagram" | "sequencediagram" { SequenceParser(parser, SEQ_SD, NULL, ctx); goto loop; }
        "participant" | "Participant" { SequenceParser(parser, SEQ_PARTICIPANT, NULL, ctx); goto loop; }
        "actor" | "Actor"           { SequenceParser(parser, SEQ_ACTOR_KW, NULL, ctx); goto loop; }
        "create" | "Create"          { SequenceParser(parser, SEQ_CREATE, NULL, ctx); goto loop; }
        "destroy" | "Destroy"         { SequenceParser(parser, SEQ_DESTROY, NULL, ctx); goto loop; }
        "as" | "AS"              { SequenceParser(parser, SEQ_AS, NULL, ctx); goto loop; }

        "activate" | "Activate"     { SequenceParser(parser, SEQ_ACTIVATE, NULL, ctx); goto loop; }
        "deactivate" | "Deactivate" { SequenceParser(parser, SEQ_DEACTIVATE, NULL, ctx); goto loop; }
        
        "note" | "Note"            { SequenceParser(parser, SEQ_NOTE, NULL, ctx); goto loop; }
        "left of" | "left_of"      { SequenceParser(parser, SEQ_LEFT_OF, NULL, ctx); goto loop; }
        "right of" | "right_of"    { SequenceParser(parser, SEQ_RIGHT_OF, NULL, ctx); goto loop; }
        "over" | "Over"            { SequenceParser(parser, SEQ_OVER, NULL, ctx); goto loop; }

        "box" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_BOX, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 3, s->cursor), ctx);
            goto loop; 
        }
        "loop" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_LOOP, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 4, s->cursor), ctx);
            goto loop; 
        }
        "rect" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_RECT, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 4, s->cursor), ctx);
            goto loop; 
        }
        "opt" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_OPT, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 3, s->cursor), ctx);
            goto loop; 
        }
        "alt" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_ALT, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 3, s->cursor), ctx);
            goto loop; 
        }
        "else" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_ELSE, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 4, s->cursor), ctx);
            goto loop; 
        }
        "par" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_PAR, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 3, s->cursor), ctx);
            goto loop; 
        }
        "and" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_AND, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 3, s->cursor), ctx);
            goto loop; 
        }
        "critical" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_CRITICAL, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 8, s->cursor), ctx);
            goto loop; 
        }
        "option" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_OPTION, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 6, s->cursor), ctx);
            goto loop; 
        }
        "break" [ \t]+ [^#\n;\x00]* {
            SequenceParser(parser, SEQ_BREAK, NULL, ctx); 
            SequenceParser(parser, SEQ_TXT, trim_label(token + 5, s->cursor), ctx);
            goto loop; 
        }
        "end" | "End"   { SequenceParser(parser, SEQ_END, NULL, ctx); goto loop; }

        "autonumber" | "Autonumber" { SequenceParser(parser, SEQ_AUTONUMBER, NULL, ctx); goto loop; }
        
        ","               { SequenceParser(parser, SEQ_COMMA, NULL, ctx); goto loop; }
        "+"               { SequenceParser(parser, SEQ_PLUS, NULL, ctx); goto loop; }
        "-"               { SequenceParser(parser, SEQ_MINUS, NULL, ctx); goto loop; }
        
        // Arrows
        "<<-->>" { SequenceParser(parser, SEQ_ARROW, strdup("<<-->>"), ctx); goto loop; }
        "<<->>"  { SequenceParser(parser, SEQ_ARROW, strdup("<<->>"), ctx); goto loop; }
        "-->>"   { SequenceParser(parser, SEQ_ARROW, strdup("-->>"), ctx); goto loop; }
        "->>"    { SequenceParser(parser, SEQ_ARROW, strdup("->>"), ctx); goto loop; }
        "-->"    { SequenceParser(parser, SEQ_ARROW, strdup("-->"), ctx); goto loop; }
        "->"     { SequenceParser(parser, SEQ_ARROW, strdup("->"), ctx); goto loop; }
        "--x"    { SequenceParser(parser, SEQ_ARROW, strdup("--x"), ctx); goto loop; }
        "-x"     { SequenceParser(parser, SEQ_ARROW, strdup("-x"), ctx); goto loop; }
        "--))"   { SequenceParser(parser, SEQ_ARROW, strdup("--))"), ctx); goto loop; }
        "-))"    { SequenceParser(parser, SEQ_ARROW, strdup("-))"), ctx); goto loop; }

        // Message text (starts with :)
        ":" [^#\n;\x00]* {
            SequenceParser(parser, SEQ_TXT, trim_label(token, s->cursor), ctx);
            goto loop;
        }

        "," { SequenceParser(parser, SEQ_COMMA, NULL, ctx); goto loop; }
        "+" { SequenceParser(parser, SEQ_PLUS, NULL, ctx); goto loop; }
        "-" { SequenceParser(parser, SEQ_MINUS, NULL, ctx); goto loop; }

        ident {
            SequenceParser(parser, SEQ_IDENTIFIER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        string {
            SequenceParser(parser, SEQ_STRING, copy_token(token + 1, s->cursor - 1), ctx);
            goto loop;
        }

        // Grammar productions terminate statements with NEWLINE. Treat EOF as
        // a line terminator so files without a final newline parse identically.
        "\000" { SequenceParser(parser, SEQ_NEWLINE, NULL, ctx); return; }
        * { goto loop; }
    */
}
