#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timeline/timeline_ast.h"
#include "timeline_parser_gen.h"

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

static char* trim_copy(const char* start, const char* end) {
    while(start < end && (*start == ' ' || *start == '\t')) start++;
    while(end > start && (*(end-1) == ' ' || *(end-1) == '\t' || *(end-1) == '\r' || *(end-1) == '\n')) end--;
    return copy_token(start, end);
}

void timeline_scan(Scanner *s, void *parser, TimelineParserContext *ctx) {
    const char *token;

    loop:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        TimelineParser(parser, TL_EOF, NULL, ctx);
        return;
    }
    
    token = s->cursor;
    
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;

        ws = [ \t]+;
        newline = "\r\n" | "\r" | "\n";
        
        ws { goto loop; }
        newline { 
            s->line++; 
            TimelineParser(parser, TL_NL, NULL, ctx); 
            goto loop; 
        }
        
        "%%" [^\r\n]* { goto loop; }
        "#" [^\r\n]* { goto loop; }

        "timeline" / ([ \t\r\n\000]) { TimelineParser(parser, TL_TIMELINE_KW, NULL, ctx); goto loop; }
        
        "title" [ \t]+ ([^\r\n]*) {
            TimelineParser(parser, TL_TITLE_VAL, trim_copy(token + 5, s->cursor), ctx);
            goto loop;
        }

        "section" [ \t]+ ([^:\r\n]*) {
            TimelineParser(parser, TL_SECTION_VAL, trim_copy(token + 7, s->cursor), ctx);
            goto loop;
        }

        "accTitle" [ \t]* ":" [ \t]* ([^\r\n]*) {
             const char* p = strchr(token, ':');
             if (p) TimelineParser(parser, TL_ACC_TITLE_VAL, trim_copy(p + 1, s->cursor), ctx);
             goto loop;
        }

        "accDescr" [ \t]* ":" [ \t]* ([^\r\n]*) {
             const char* p = strchr(token, ':');
             if (p) TimelineParser(parser, TL_ACC_DESCR_VAL, trim_copy(p + 1, s->cursor), ctx);
             goto loop;
        }

        ":" [ \t]+ ([^\r\n]*) {
            TimelineParser(parser, TL_EVENT_VAL, trim_copy(token + 1, s->cursor), ctx);
            goto loop;
        }

        "\000" { TimelineParser(parser, TL_EOF, NULL, ctx); return; }

        // Period line
        ([^ \t\r\n:\000#%] [^\r\n\000]*) {
             const char* sep = NULL;
             const char* p = token;
             while(p < s->cursor - 1) {
                 if(*p == ' ' && *(p+1) == ':' && p+2 < s->cursor && *(p+2) == ' ') {
                     sep = p;
                     break;
                 }
                 p++;
             }
             if(sep) {
                 TimelineParser(parser, TL_PERIOD_VAL, trim_copy(token, sep), ctx);
                 TimelineParser(parser, TL_EVENT_VAL, trim_copy(sep + 2, s->cursor), ctx);
             } else {
                 TimelineParser(parser, TL_PERIOD_VAL, trim_copy(token, s->cursor), ctx);
             }
             goto loop;
        }
        * { s->cursor++; goto loop; }
    */
}
