#ifndef FLOWCHART_LEXER_H
#define FLOWCHART_LEXER_H

#include <stdlib.h>
#include <string.h>
#include "flowchart_parser_gen.h"
#include "flowchart/flowchart_parser_wrapper.h"

typedef struct {
    char *cursor;
    char *limit;
    char *marker;
    int line;
} LexerState;

static char* copy_token(const char* start, const char* end) {
    size_t len = end - start;
    char* s = (char*)malloc(len + 1);
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

void flowchart_scan(LexerState *s, void *parser, FlowchartParserContext *ctx) {
    char *token;
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
        // 限制标识符，不包括特殊操作符字符，或者将操作符放在前面
        ident = [a-zA-Z0-9_][a-zA-Z0-9_]*; 
        string = "\"" [^"\000]* "\"";
        comment = "%%" [^\n\000]*;

        white { goto loop; }
        newline { s->line++; FlowchartParser(parser, FC_NEWLINE, NULL, ctx); goto loop; }
        comment { goto loop; }

        // 优先级 1: 关键字
        "graph"      { FlowchartParser(parser, FC_GRAPH, NULL, ctx); goto loop; }
        "flowchart"  { FlowchartParser(parser, FC_FLOWCHART, NULL, ctx); goto loop; }
        "subgraph"   { FlowchartParser(parser, FC_SUBGRAPH, NULL, ctx); goto loop; }
        "end"        { FlowchartParser(parser, FC_END, NULL, ctx); goto loop; }
        "class"      { FlowchartParser(parser, FC_CLASS, NULL, ctx); goto loop; }
        "LR" | "TB" | "BT" | "RL" | "TD" { 
            FlowchartParser(parser, FC_DIR, copy_token(token, s->cursor), ctx); 
            goto loop; 
        }

        // 优先级 2: 多字符箭头（必须在标识符前匹配）
        "-->"              { FlowchartParser(parser, FC_ARROW, strdup("-->"), ctx); goto loop; }
        "---"              { FlowchartParser(parser, FC_ARROW, strdup("---"), ctx); goto loop; }
        "--"               { FlowchartParser(parser, FC_ARROW, strdup("--"), ctx); goto loop; }
        "->"               { FlowchartParser(parser, FC_ARROW, strdup("->"), ctx); goto loop; }
        "==>"              { FlowchartParser(parser, FC_ARROW, strdup("==>"), ctx); goto loop; }
        "==="              { FlowchartParser(parser, FC_ARROW, strdup("==="), ctx); goto loop; }

        // 优先级 3: 形状限定符
        "[["              { FlowchartParser(parser, FC_SUBROUTINE_START, NULL, ctx); goto loop; }
        "]]"              { FlowchartParser(parser, FC_SUBROUTINE_END, NULL, ctx); goto loop; }
        "{{"              { FlowchartParser(parser, FC_HEXAGON_START, NULL, ctx); goto loop; }
        "}}"              { FlowchartParser(parser, FC_HEXAGON_END, NULL, ctx); goto loop; }
        "(("              { FlowchartParser(parser, FC_DOUBLE_CIRCLE_START, NULL, ctx); goto loop; }
        "))"              { FlowchartParser(parser, FC_DOUBLE_CIRCLE_END, NULL, ctx); goto loop; }
        "(["              { FlowchartParser(parser, FC_STADIUM_START, NULL, ctx); goto loop; }
        "])"              { FlowchartParser(parser, FC_STADIUM_END, NULL, ctx); goto loop; }
        "["               { FlowchartParser(parser, FC_SQS, NULL, ctx); goto loop; }
        "]"               { FlowchartParser(parser, FC_SQE, NULL, ctx); goto loop; }
        "("               { FlowchartParser(parser, FC_PS, NULL, ctx); goto loop; }
        ")"               { FlowchartParser(parser, FC_PE, NULL, ctx); goto loop; }
        "{"               { FlowchartParser(parser, FC_RHOMBUS_START, NULL, ctx); goto loop; }
        "}"               { FlowchartParser(parser, FC_RHOMBUS_END, NULL, ctx); goto loop; }
        "|"               { FlowchartParser(parser, FC_PIPE, NULL, ctx); goto loop; }
        ";"               { FlowchartParser(parser, FC_SEMI, NULL, ctx); goto loop; }

        // 优先级 4: 通用标识符
        ident {
            FlowchartParser(parser, FC_IDENTIFIER, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        string {
            char *val = copy_token(token + 1, s->cursor - 1);
            FlowchartParser(parser, FC_STRING, val, ctx);
            goto loop;
        }

        "\000" { FlowchartParser(parser, FC_EOF, NULL, ctx); return; }

        . { goto loop; }
    */
}

#endif
