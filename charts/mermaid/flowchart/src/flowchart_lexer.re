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
    /* 1=node id, 2=target, 3=optional tooltip, 4=tooltip consumed. */
    int click_phase;
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
        // Allow dotted and hyphenated node IDs without swallowing edge arrows.
        ident = [a-zA-Z0-9_]+ ([.-] [a-zA-Z0-9_]+)*;
        string = "\"" [^"\000]* "\"";
        html = "<" [^>\n\000]* ">";
        comment = "%%" [^\n\000]*;
        punct = [.!?]+;
        // Mermaid also permits --label--> and -- label --> edges.
        inline_edge_label = "--" [ \t]* ident [ \t]* "-->";

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
            s->click_phase = 0;
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
        "click"      { s->click_phase = 1; FlowchartParser(parser, FC_CLICK, NULL, ctx); goto loop; }
        "class"      { FlowchartParser(parser, FC_CLASS, NULL, ctx); goto loop; }
        "direction"  { FlowchartParser(parser, FC_DIRECTION, NULL, ctx); goto loop; }
        "LR" | "TB" | "BT" | "RL" | "TD" { 
            FlowchartParser(parser, FC_DIR, copy_token(token, s->cursor), ctx); 
            goto loop; 
        }

        // Inline labels are one edge, not two unlabeled edges with a fake node.
        inline_edge_label {
            const char* begin = token + 2;
            const char* end = s->cursor - 3;
            while (begin < end && (*begin == ' ' || *begin == '\t')) ++begin;
            while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) --end;
            FlowchartParser(parser, FC_ARROW, strdup("-->"), ctx);
            FlowchartParser(parser, FC_PIPE, NULL, ctx);
            FlowchartParser(parser, FC_IDENTIFIER, copy_token(begin, end), ctx);
            FlowchartParser(parser, FC_PIPE, NULL, ctx);
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
        "[/"               { FlowchartParser(parser, FC_TRAP_START, NULL, ctx); goto loop; }
        "/]"               { FlowchartParser(parser, FC_TRAP_END, NULL, ctx); goto loop; }
        "\x5B\x5C"         { FlowchartParser(parser, FC_INV_TRAP_START, NULL, ctx); goto loop; }
        "\x5C\x5D"         { FlowchartParser(parser, FC_TRAP_END, NULL, ctx); goto loop; }
        ">"                { FlowchartParser(parser, FC_ODD_START, NULL, ctx); goto loop; }
        "[["              { FlowchartParser(parser, FC_SUBROUTINE_START, NULL, ctx); goto loop; }
        "]]"              { FlowchartParser(parser, FC_SUBROUTINE_END, NULL, ctx); goto loop; }
        "{{"              { FlowchartParser(parser, FC_HEXAGON_START, NULL, ctx); goto loop; }
        "}}"              { FlowchartParser(parser, FC_HEXAGON_END, NULL, ctx); goto loop; }
        // Mermaid ((label)) is a circle; only (((label))) is a double circle.
        "((("             { FlowchartParser(parser, FC_DOUBLE_CIRCLE_START, NULL, ctx); goto loop; }
        ")))"             { FlowchartParser(parser, FC_DOUBLE_CIRCLE_END, NULL, ctx); goto loop; }
        "(("              { FlowchartParser(parser, FC_CPS, NULL, ctx); goto loop; }
        "))"              { FlowchartParser(parser, FC_CPE, NULL, ctx); goto loop; }
        // The cylinder spelling [(label)] must outrank single brackets.
        "[("              { FlowchartParser(parser, FC_CYLINDER_START, NULL, ctx); goto loop; }
        ")]"              { FlowchartParser(parser, FC_CYLINDER_END, NULL, ctx); goto loop; }
        "(["              { FlowchartParser(parser, FC_STADIUM_START, NULL, ctx); goto loop; }
        "])"              { FlowchartParser(parser, FC_STADIUM_END, NULL, ctx); goto loop; }
        "["               { FlowchartParser(parser, FC_SQS, NULL, ctx); goto loop; }
        "]"               { FlowchartParser(parser, FC_SQE, NULL, ctx); goto loop; }
        "("               { FlowchartParser(parser, FC_PS, NULL, ctx); goto loop; }
        ")"               { FlowchartParser(parser, FC_PE, NULL, ctx); goto loop; }
        "{"               { FlowchartParser(parser, FC_RHOMBUS_START, NULL, ctx); goto loop; }
        "}"               { FlowchartParser(parser, FC_RHOMBUS_END, NULL, ctx); goto loop; }
        "|"               { FlowchartParser(parser, FC_PIPE, NULL, ctx); goto loop; }
        "&"               { FlowchartParser(parser, FC_AMP, NULL, ctx); goto loop; }
        ";"               { FlowchartParser(parser, FC_SEMI, NULL, ctx); s->in_classdef = 0; s->click_phase = 0; goto loop; }
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

        // Preserve label punctuation instead of silently dropping it.
        punct {
            FlowchartParser(parser, FC_PUNCT, copy_token(token, s->cursor), ctx);
            goto loop;
        }

        // 优先级 4: 通用标识符
        ident {
            char *value = copy_token(token, s->cursor);
            if (s->click_phase == 1) s->click_phase = 2;
            else if (s->click_phase == 2) s->click_phase = 3;
            FlowchartParser(parser, FC_IDENTIFIER, value, ctx);
            goto loop;
        }

        string {
            char *val = copy_token(token + 1, s->cursor - 1);
            if (s->click_phase == 1) {
                /* Quoted node id. */
                s->click_phase = 2;
            } else if (s->click_phase == 2) {
                /* Quoted href target; a following string is the tooltip. */
                s->click_phase = 3;
            } else if (s->click_phase == 3) {
                /* Mermaid click tooltip is intentionally omitted from the
                   normalized JSON contract. Consume it here to avoid treating
                   it as a free-standing node or shifting grammar reductions. */
                s->click_phase = 4;
                free(val);
                goto loop;
            }
            FlowchartParser(parser, FC_STRING, val, ctx);
            goto loop;
        }

        "\000" { FlowchartParser(parser, FC_EOF, NULL, ctx); return; }

        . { ctx->error_count++; goto loop; }
    */
}

#endif
