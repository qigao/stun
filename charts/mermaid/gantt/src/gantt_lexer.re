#include <stdlib.h>
#include <string.h>
#include "gantt/gantt_ast.h"
#include "gantt_parser_gen.h"

void GanttParser(void *parser, int token, void *value, GanttParserContext *ctx);

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

static char* copy_quoted(const char* start, const char* end) {
    if (end - start < 2) return strdup("");
    size_t len = (end - start) - 2;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start + 1, len);
    res[len] = '\0';
    return res;
}

void gantt_scan(Scanner *s, void *parser, GanttParserContext *ctx) {
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
        comment = ("%%"| "#") [^\n]* newline;

        white { goto loop; }
        newline { s->line++; GanttParser(parser, GANTT_NL, NULL, ctx); goto loop; }
        comment { s->line++; GanttParser(parser, GANTT_NL, NULL, ctx); goto loop; }

        "gantt"             { GanttParser(parser, GANTT_GANTT, NULL, ctx); goto loop; }
        "title"             { GanttParser(parser, GANTT_TITLE_KW, NULL, ctx); goto loop; }
        "dateFormat"        { GanttParser(parser, GANTT_DATE_FORMAT_KW, NULL, ctx); goto loop; }
        "axisFormat"        { GanttParser(parser, GANTT_AXIS_FORMAT_KW, NULL, ctx); goto loop; }
        "section"           { GanttParser(parser, GANTT_SECTION_KW, NULL, ctx); goto loop; }
        "excludes"          { GanttParser(parser, GANTT_EXCLUDES_KW, NULL, ctx); goto loop; }
        "inclusiveEndDates" { GanttParser(parser, GANTT_INCLUSIVE_END_DATES, NULL, ctx); goto loop; }
        "topAxis"           { GanttParser(parser, GANTT_TOP_AXIS, NULL, ctx); goto loop; }

        // Keywords for status
        "done"      { GanttParser(parser, GANTT_STATUS_DONE_KW, NULL, ctx); goto loop; }
        "active"    { GanttParser(parser, GANTT_STATUS_ACTIVE_KW, NULL, ctx); goto loop; }
        "crit"      { GanttParser(parser, GANTT_STATUS_CRIT_KW, NULL, ctx); goto loop; }
        "milestone" { GanttParser(parser, GANTT_STATUS_MILESTONE_KW, NULL, ctx); goto loop; }

        ":" { GanttParser(parser, GANTT_COLON, NULL, ctx); goto loop; }
        "," { GanttParser(parser, GANTT_COMMA, NULL, ctx); goto loop; }

        // Quoted string
        "\"" [^\"]* "\"" {
            GanttParser(parser, GANTT_STRING, copy_quoted(token, s->cursor), ctx);
            goto loop;
        }

        // Identifier/Text
        [a-zA-Z0-9_\-/%]+ {
            GanttParser(parser, GANTT_TEXT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        "\000" { return; }
        * { goto loop; }
    */
}
