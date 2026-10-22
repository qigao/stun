#ifndef FLOWCHART_LEXER_H
#define FLOWCHART_LEXER_H

#include <stdlib.h>
#include <stdint.h>
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
    int shape_label_depth;
    int after_subgraph_keyword;
    const char* subgraph_extra_gap_start;
    size_t subgraph_extra_gap_size;
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

static char* copy_subgraph_name_token(LexerState* s,
                                      const char* start, const char* end) {
    size_t prefix = s->after_subgraph_keyword ? s->subgraph_extra_gap_size : 0;
    const char* gap = s->subgraph_extra_gap_start;
    s->after_subgraph_keyword = 0;
    s->subgraph_extra_gap_size = 0;
    s->subgraph_extra_gap_start = NULL;
    if (!prefix) return copy_token(start, end);
    size_t length = (size_t)(end - start);
    if (!gap || prefix >= SIZE_MAX - length) return NULL;
    char* result = (char*)malloc(prefix + length + 1);
    if (!result) return NULL;
    memcpy(result, gap, prefix);
    memcpy(result + prefix, start, length);
    result[prefix + length] = '\0';
    return result;
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
        class_shortcut = ":::" ident;
        string = "\"" [^"\000]* "\"";
        html = "<" [^>\n\000]* ">";
        comment = "%%" [^\n\000]*;
        punct = [.!?]+;
        // Mermaid also permits --label--> and -- label --> edges.
        label_words = ident ([ \t]+ ident)*;
        inline_edge_label = ("--" | "<--") [ \t]* label_words [ \t]* ("-->" | "--->");
        variable_dash_arrow = "--" "-"* ">";
        variable_thick_arrow = "==" "="* ">";
        // Named edge IDs are presentation metadata; do not turn them into
        // source/target nodes. Preserve the underlying arrow operator.
        edge_id_arrow = ident "@" (variable_dash_arrow | variable_thick_arrow);
        edge_curve_props = ident "@{" [ \t]* "curve:" [^}\n\000]* "}";
        // link@-- "Label" --> is an explicitly named, labeled edge.
        named_quoted_edge_label = ident "@" "--" [ \t]* string [ \t]* "-->";

        white {
            /* Mermaid consumes one separator after "subgraph"; any remaining
               spaces belong to the unquoted title, not to the stable ID. */
            if (s->after_subgraph_keyword && !s->subgraph_extra_gap_start &&
                s->cursor > token) {
                s->subgraph_extra_gap_start = token + 1;
                s->subgraph_extra_gap_size = (size_t)(s->cursor - token - 1);
            }
            if (s->shape_label_depth > 0) {
                char* whitespace = copy_token(token, s->cursor);
                if (whitespace)
                    FlowchartParser(parser, FC_PUNCT, whitespace, ctx);
                else
                    ctx->error_count++;
            }
            goto loop;
        }
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
            s->after_subgraph_keyword = 0;
            s->subgraph_extra_gap_start = NULL;
            s->subgraph_extra_gap_size = 0;
            goto loop;
        }
        comment { goto loop; }

        // v2 node metadata blocks do not change the graph facts projected by
        // this adapter yet; consume the whole block rather than leaking its
        // label/value tokens as phantom nodes.
        edge_curve_props {
            /* Resolve after the preceding edge's Lemon reduction. Looking
               up the ID directly in the scanner can miss that pending edge. */
            FlowchartParser(parser, FC_EDGE_PROPS,
                            copy_token(token, s->cursor), ctx);
            goto loop;
        }
        new_node_props { goto loop; }
        // Style declarations may address a subgraph ID as a node, even when
        // that ID has never been an edge endpoint. Keep just the target ID in
        // the AST and consume the CSS tail as one token so property fragments
        // cannot turn into phantom nodes or edges.
        style_stmt {
            const char* id = token + 5; /* strlen("style") */
            while (id < s->cursor && (*id == ' ' || *id == '\t')) ++id;
            const char* end = id;
            while (end < s->cursor && *end != ' ' && *end != '\t') ++end;
            if (end > id)
                FlowchartParser(parser, FC_IDENTIFIER, copy_token(id, end), ctx);
            else
                ctx->error_count++;
            goto loop;
        }

        // 优先级 1: 关键字
        "graph"      { FlowchartParser(parser, FC_GRAPH, NULL, ctx); goto loop; }
        "flowchart"  { FlowchartParser(parser, FC_FLOWCHART, NULL, ctx); goto loop; }
        "subgraph"   {
            s->after_subgraph_keyword = 1;
            s->subgraph_extra_gap_start = NULL;
            s->subgraph_extra_gap_size = 0;
            FlowchartParser(parser, FC_SUBGRAPH, NULL, ctx);
            goto loop;
        }
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

        // An explicit Mermaid edge ID (e.g. e1@-->) is not a node.
        edge_id_arrow {
            const char* at = (const char*)memchr(token, '@', (size_t)(s->cursor - token));
            if (at)
                FlowchartParser(parser, FC_ARROW, copy_token(token, s->cursor), ctx);
            else
                ctx->error_count++;
            goto loop;
        }

        // Consume the complete named edge before the lexer can emit its ID as
        // a phantom node. The edge ID is metadata, not a source endpoint.
        named_quoted_edge_label {
            const char* quote = (const char*)memchr(token, '"',
                                   (size_t)(s->cursor - token));
            if (!quote || quote >= s->cursor - 3) {
                ctx->error_count++;
                goto loop;
            }
            const char* closing = (const char*)memchr(quote + 1, '"',
                                   (size_t)(s->cursor - quote - 1));
            if (!closing) {
                ctx->error_count++;
                goto loop;
            }
            const char* at = (const char*)memchr(token, '@',
                                               (size_t)(quote - token));
            if (!at) { ctx->error_count++; goto loop; }
            size_t prefix = (size_t)(at - token) + 1;
            char* arrow = (char*)malloc(prefix + 4);
            if (!arrow) { ctx->error_count++; goto loop; }
            memcpy(arrow, token, prefix);
            memcpy(arrow + prefix, "-->", 4);
            FlowchartParser(parser, FC_ARROW, arrow, ctx);
            FlowchartParser(parser, FC_PIPE, NULL, ctx);
            FlowchartParser(parser, FC_STRING, copy_token(quote + 1, closing), ctx);
            FlowchartParser(parser, FC_PIPE, NULL, ctx);
            goto loop;
        }

        // Inline labels are one edge, not two unlabeled edges with a fake node.
        inline_edge_label {
            const char* begin = token + (token[0] == '<' ? 3 : 2);
            const size_t width = (size_t)(s->cursor - token);
            const int long_tip = width >= 4 &&
                                 memcmp(s->cursor - 4, "--->", 4) == 0;
            const char* end = s->cursor - (long_tip ? 4 : 3);
            while (begin < end && (*begin == ' ' || *begin == '\t')) ++begin;
            while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) --end;
            FlowchartParser(parser, FC_ARROW, strdup(long_tip ? "--->" : "-->"), ctx);
            FlowchartParser(parser, FC_PIPE, NULL, ctx);
            FlowchartParser(parser, FC_IDENTIFIER, copy_token(begin, end), ctx);
            FlowchartParser(parser, FC_PIPE, NULL, ctx);
            goto loop;
        }

        // Dot-stroke Mermaid operators, including bidirectional notation.
        ".->"              { FlowchartParser(parser, FC_ARROW, strdup(".->"), ctx); goto loop; }
        "<.->"             { FlowchartParser(parser, FC_ARROW, strdup("<.->"), ctx); goto loop; }
        "<-...->"          { FlowchartParser(parser, FC_ARROW, strdup("<-...->"), ctx); goto loop; }

        // 优先级 2: 多字符箭头（必须在标识符前匹配）
        // Preserve the full operator width and its tip for spacing/length.
        variable_dash_arrow {
            FlowchartParser(parser, FC_ARROW, copy_token(token, s->cursor), ctx);
            goto loop;
        }
        variable_thick_arrow {
            FlowchartParser(parser, FC_ARROW, copy_token(token, s->cursor), ctx);
            goto loop;
        }
        "-->"              { FlowchartParser(parser, FC_ARROW, strdup("-->"), ctx); goto loop; }
        "---"              { FlowchartParser(parser, FC_ARROW, strdup("---"), ctx); goto loop; }
        "--"               { FlowchartParser(parser, FC_ARROW, strdup("--"), ctx); goto loop; }
        "->"               { FlowchartParser(parser, FC_ARROW, strdup("->"), ctx); goto loop; }
        "===>"             { FlowchartParser(parser, FC_ARROW, strdup("==="), ctx); goto loop; }
        "==>"              { FlowchartParser(parser, FC_ARROW, strdup("==>"), ctx); goto loop; }
        "==="              { FlowchartParser(parser, FC_ARROW, strdup("==="), ctx); goto loop; }

        // 优先级 3: 形状限定符
        "[/"               { s->shape_label_depth++; FlowchartParser(parser, FC_TRAP_START, NULL, ctx); goto loop; }
        "/]"               { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_TRAP_END, NULL, ctx); goto loop; }
        "\x5B\x5C"         { s->shape_label_depth++; FlowchartParser(parser, FC_INV_TRAP_START, NULL, ctx); goto loop; }
        "\x5C\x5D"         { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_TRAP_BACKSLASH_END, NULL, ctx); goto loop; }
        ">"                { s->shape_label_depth++; FlowchartParser(parser, FC_ODD_START, NULL, ctx); goto loop; }
        "[["              { s->shape_label_depth++; FlowchartParser(parser, FC_SUBROUTINE_START, NULL, ctx); goto loop; }
        "]]"              { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_SUBROUTINE_END, NULL, ctx); goto loop; }
        "{{"              { s->shape_label_depth++; FlowchartParser(parser, FC_HEXAGON_START, NULL, ctx); goto loop; }
        "}}"              { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_HEXAGON_END, NULL, ctx); goto loop; }
        // Mermaid ((label)) is a circle; only (((label))) is a double circle.
        "((("             { s->shape_label_depth++; FlowchartParser(parser, FC_DOUBLE_CIRCLE_START, NULL, ctx); goto loop; }
        ")))"             { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_DOUBLE_CIRCLE_END, NULL, ctx); goto loop; }
        "(("              { s->shape_label_depth++; FlowchartParser(parser, FC_CPS, NULL, ctx); goto loop; }
        "))"              { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_CPE, NULL, ctx); goto loop; }
        // The cylinder spelling [(label)] must outrank single brackets.
        "[("              { s->shape_label_depth++; FlowchartParser(parser, FC_CYLINDER_START, NULL, ctx); goto loop; }
        ")]"              { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_CYLINDER_END, NULL, ctx); goto loop; }
        "(["              { s->shape_label_depth++; FlowchartParser(parser, FC_STADIUM_START, NULL, ctx); goto loop; }
        "])"              { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_STADIUM_END, NULL, ctx); goto loop; }
        "["               { s->shape_label_depth++; FlowchartParser(parser, FC_SQS, NULL, ctx); goto loop; }
        "]"               { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_SQE, NULL, ctx); goto loop; }
        "("               { s->shape_label_depth++; FlowchartParser(parser, FC_PS, NULL, ctx); goto loop; }
        ")"               { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_PE, NULL, ctx); goto loop; }
        "{"               { s->shape_label_depth++; FlowchartParser(parser, FC_RHOMBUS_START, NULL, ctx); goto loop; }
        "}"               { if (s->shape_label_depth > 0) --s->shape_label_depth; FlowchartParser(parser, FC_RHOMBUS_END, NULL, ctx); goto loop; }
        class_shortcut {
            FlowchartParser(parser, FC_CLASS_SHORTCUT, copy_token(token + 3, s->cursor), ctx);
            goto loop;
        }
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
            char *value = copy_subgraph_name_token(s, token, s->cursor);
            if (!value) { ctx->error_count++; goto loop; }
            if (s->click_phase == 1) s->click_phase = 2;
            else if (s->click_phase == 2) s->click_phase = 3;
            FlowchartParser(parser, FC_IDENTIFIER, value, ctx);
            goto loop;
        }

        string {
            char *val = copy_subgraph_name_token(s, token + 1, s->cursor - 1);
            if (!val) { ctx->error_count++; goto loop; }
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
