#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "journey/journey_ast.h"
#include "journey_parser_gen.h"

void JourneyParser(void *parser, int token, void *value, JourneyParserContext *ctx);

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
    int at_bol;
} Scanner;

static char* copy_token(const char* start, const char* end) {
    size_t len = end - start;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    return res;
}

void journey_scan(Scanner *s, void *parser, JourneyParserContext *ctx) {
    const char *token;

    loop:
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        newline = [\n];
        ws = [ \t\r]+;
        comment = ("%%"| "#") [^\n]*;

        newline { 
            s->line++; 
            s->at_bol = 1; 
            JourneyParser(parser, JOURNEY_NEWLINE, NULL, ctx); 
            goto loop; 
        }
        
        comment { goto loop; }
        
        ws { goto loop; }

        "journey" { 
            JourneyParser(parser, JOURNEY_JOURNEY, NULL, ctx); 
            goto loop; 
        }

        "title" [ \t] [^\r\n#;\x00]+ {
            char* val = copy_token(token, s->cursor);
            JourneyParser(parser, JOURNEY_TITLE, val, ctx);
            goto loop;
        }

        "accTitle" [ \t]* ":" [ \t]* {
            JourneyParser(parser, JOURNEY_ACC_TITLE, NULL, ctx);
            goto acc_title_state;
        }

        "accDescr" [ \t]* ":" [ \t]* {
            JourneyParser(parser, JOURNEY_ACC_DESCR, NULL, ctx);
            goto acc_descr_state;
        }

        "accDescr" [ \t]* "{" [ \t]* {
            goto acc_descr_multiline_state;
        }

        "section" [ \t] [^\r\n#;:\x00]+ {
            char* val = copy_token(token, s->cursor);
            JourneyParser(parser, JOURNEY_SECTION, val, ctx);
            goto loop;
        }

        ":" [^\r\n#;\x00]* {
            char* val = copy_token(token, s->cursor);
            JourneyParser(parser, JOURNEY_TASK_DATA, val, ctx);
            goto loop;
        }

        [^ \t\r\n#;:\x00] [^\r\n#;:\x00]* {
            char* val = copy_token(token, s->cursor);
            JourneyParser(parser, JOURNEY_TASK_NAME, val, ctx);
            goto loop;
        }

        "\000" { return; }
        * { goto loop; }
    */

    acc_title_state:
    token = s->cursor;
    /*!re2c
        [^\r\n#;\x00]* {
            char* val = copy_token(token, s->cursor);
            JourneyParser(parser, JOURNEY_ACC_TITLE_VALUE, val, ctx);
            goto loop;
        }
    */

    acc_descr_state:
    token = s->cursor;
    /*!re2c
        [^\r\n#;\x00]* {
            char* val = copy_token(token, s->cursor);
            JourneyParser(parser, JOURNEY_ACC_DESCR_VALUE, val, ctx);
            goto loop;
        }
    */

    acc_descr_multiline_state:
    token = s->cursor;
    /*!re2c
        "}" {
             // Empty content case?
             // If we just see }, we emit empty value?
             // Or maybe we treat it as close.
             // But parser expects ACC_DESCR_MULTILINE_VALUE.
             // If content is empty, emit empty string.
             // But if we matched content below, we would have emitted.
             // This rule matches ONLY "}".
             // So it means empty content.
             char* val = strdup("");
             JourneyParser(parser, JOURNEY_ACC_DESCR_MULTILINE_VALUE, val, ctx);
             goto loop;
        }

        ([^}]+) "}" {
             // Content + }
             // Only copy content
             char* val = copy_token(token, s->cursor - 1);
             JourneyParser(parser, JOURNEY_ACC_DESCR_MULTILINE_VALUE, val, ctx);
             goto loop;
        }
    */
}
