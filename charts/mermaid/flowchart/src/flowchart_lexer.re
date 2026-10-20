#ifndef FLOWCHART_LEXER_H
#define FLOWCHART_LEXER_H

#include <stdlib.h>
#include <string.h>
#include "flowchart_parser_gen.h"
#include "flowchart/flowchart_parser_wrapper.h"

typedef struct {
    char *start;
    char *cursor;
    char *limit;
    char *marker;
    int line;
    int in_classdef;
} LexerState;

void FlowchartParser(void *parser, int token, void *value,
                     FlowchartParserContext *ctx);

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
        style = [a-zA-Z_][a-zA-Z0-9_-]* ":" [^,;\n\000]+;
        new_node_props = "@{" [^}\n\000]* "}";
        style_stmt = "style" [ \t]+ [^\n\000]*;
        // 限制标识符，不包括特殊操作符字符，或者将操作符放在前面
        ident = [a-zA-Z0-9_][a-zA-Z0-9_]*; 
        string = "\"" [^"\000]* "\"";
        html = "<" [^>\n\000]* ">";
        comment = "%%" [^\n\000]*;

        white { goto loop; }
        newline {
            s->line++;
            /* Mermaid permits an edge operator to begin the next physical
               line, e.g. a{{label}}\n--> b. Keep that newline out of the
               grammar only when the next non-whitespace byte starts an edge. */
            char *lookahead = s->cursor;
            while (*lookahead == ' ' || *lookahead == '\t' || *lookahead == '\r')
                ++lookahead;
            if (*lookahead != '-' && *lookahead != '=')
                FlowchartParser(parser, FC_NEWLINE, NULL, ctx);
            s->in_classdef = 0;
            goto loop;
        }
        comment { goto loop; }

        // v2 node metadata blocks do not change the graph facts projected by
        // this adapter yet; consume the whole block rather than leaking its
        // label/value tokens as phantom nodes.
        new_node_props { goto loop; }
        // Node-local style declarations are currently outside the JSON graph
        // contract; skip their entire physical line instead of parsing CSS as nodes.
        style_stmt { goto loop; }

        // 优先级 1: 关键字
        "graph"      { FlowchartParser(parser, FC_GRAPH, NULL, ctx); goto loop; }
        "flowchart"  { FlowchartParser(parser, FC_FLOWCHART, NULL, ctx); goto loop; }
        "subgraph"   { FlowchartParser(parser, FC_SUBGRAPH, NULL, ctx); goto loop; }
        "end"        { FlowchartParser(parser, FC_END, NULL, ctx); goto loop; }
        "classDef"   { s->in_classdef = 1; FlowchartParser(parser, FC_CLASSDEF, NULL, ctx); goto loop; }
        "linkStyle"  { s->in_classdef = 1; FlowchartParser(parser, FC_LINKSTYLE, NULL, ctx); goto loop; }
        "click"      { FlowchartParser(parser, FC_CLICK, NULL, ctx); goto loop; }
        "class"      { FlowchartParser(parser, FC_CLASS, NULL, ctx); goto loop; }
        "direction"  { FlowchartParser(parser, FC_DIRECTION, NULL, ctx); goto loop; }
        "LR" | "TB" | "BT" | "RL" | "TD" { 
            FlowchartParser(parser, FC_DIR, copy_token(token, s->cursor), ctx); 
            goto loop; 
        }

        // 优先级 2: 多字符箭头（必须在标识符前匹配）
        "-->"              { FlowchartParser(parser, FC_ARROW, strdup("-->"), ctx); goto loop; }
        "---"              { FlowchartParser(parser, FC_ARROW, strdup("---"), ctx); goto loop; }
        "--"               { FlowchartParser(parser, FC_ARROW, strdup("--"), ctx); goto loop; }
        "->"               { FlowchartParser(parser, FC_ARROW, strdup("->"), ctx); goto loop; }
        "===>"             { FlowchartParser(parser, FC_ARROW, strdup("==="), ctx); goto loop; }
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
        "&"               { goto loop; }
        ";"               { FlowchartParser(parser, FC_SEMI, NULL, ctx); s->in_classdef = 0; goto loop; }
        ","               { FlowchartParser(parser, FC_COMMA, NULL, ctx); goto loop; }
        ":"               { FlowchartParser(parser, FC_COLON, NULL, ctx); goto loop; }
        "-"               { FlowchartParser(parser, FC_DASH, NULL, ctx); goto loop; }

        html {
            FlowchartParser(parser, FC_HTML, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        // CSS key:value items are kept whole only inside a classDef declaration.
        // Otherwise split at ':' to retain existing colon-separated node labels.
        style {
            if (s->in_classdef) {
                FlowchartParser(parser, FC_STYLE, copy_token(token, s->cursor), ctx);
            } else {
                char *colon = strchr(token, ':');
                if (!colon || colon == token) {
                    ctx->error_count++;
                } else {
                    FlowchartParser(parser, FC_IDENTIFIER, copy_token(token, colon), ctx);
                    s->cursor = colon;
                }
            }
            goto loop;
        }

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

        . { ctx->error_count++; goto loop; }
    */
}

#endif
