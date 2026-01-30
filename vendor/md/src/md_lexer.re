// re2c --lang c
#include "md_re2c.h"
#include "md_parser_gen.h"
#include <string>

namespace md_re2c {

int lex(LexerState* state, std::string& text) {
yyloop:
    bool is_at_line_start = (state->cursor == state->start || state->last_token == TOK_NEWLINE);
    if (!is_at_line_start && state->cursor > state->start) {
        const char* p = state->cursor - 1;
        while (p >= state->start && (*p == ' ' || *p == '\t')) { p--; }
        is_at_line_start = (p < state->start || *p == '\n');
    }

    const char* tok_start = state->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = state->cursor;
        re2c:define:YYMARKER = state->marker;
        re2c:yyfill:enable = 0;

        "\x00" { state->last_token = 0; return 0; }
        "\n"   { state->last_token = TOK_NEWLINE; return TOK_NEWLINE; }
        "\r"+  { goto yyloop; }

        // Skip leading whitespace at line start
        [ \t]+ { if (is_at_line_start) goto yyloop; else { text = std::string(tok_start, state->cursor - tok_start); state->last_token = TOK_TEXT; return TOK_TEXT; } }

        // Headers - consolidate into a single rule
        [#]{1,6} [ ]+ { 
            if (is_at_line_start) {
                int level = 0;
                while (tok_start[level] == '#') level++;
                static const int tokens[] = {0, TOK_H1, TOK_H2, TOK_H3, TOK_H4, TOK_H5, TOK_H6};
                state->last_token = tokens[level];
                return state->last_token;
            } else {
                text = std::string(tok_start, state->cursor - tok_start);
                state->last_token = TOK_TEXT;
                return TOK_TEXT;
            }
        }

        // Horizontal rules - must be the full line
        [ ]{0,3} ("---" | "***" | "___") [ ]* ("\n" | "\x00") { 
            if (is_at_line_start) { 
                // Don't consume the newline, the main loop handles it
                state->cursor = tok_start + (state->cursor - tok_start) - 1;
                state->last_token = TOK_HR; return TOK_HR; 
            } else { 
                text = std::string(tok_start, 3); state->cursor = tok_start + 3;
                state->last_token = TOK_TEXT; return TOK_TEXT; 
            } 
        }

        // Emphasis
        "**"   { text = "**"; state->last_token = TOK_DOUBLE_STAR; return TOK_DOUBLE_STAR; }
        "__"   { text = "__"; state->last_token = TOK_DOUBLE_UNDERSCORE; return TOK_DOUBLE_UNDERSCORE; }
        "~~"   { text = "~~"; state->last_token = TOK_TILDE_TILDE; return TOK_TILDE_TILDE; }
        
        // List markers
        [ ]{0,3} "- "   { if (is_at_line_start) { state->last_token = TOK_LIST_MARKER; return TOK_LIST_MARKER; } else { text = "- "; state->last_token = TOK_TEXT; return TOK_TEXT; } }
        [ ]{0,3} "* "   { if (is_at_line_start) { state->last_token = TOK_LIST_MARKER; return TOK_LIST_MARKER; } else { text = "* "; state->last_token = TOK_TEXT; return TOK_TEXT; } }
        [ ]{0,3} [0-9]+ ". " { if (is_at_line_start) { text = std::string(tok_start, state->cursor - tok_start); state->last_token = TOK_OL_MARKER; return TOK_OL_MARKER; } else { text = std::string(tok_start, state->cursor - tok_start); state->last_token = TOK_TEXT; return TOK_TEXT; } }

        "*"    { text = "*"; state->last_token = TOK_STAR; return TOK_STAR; }
        "_"    { text = "_"; state->last_token = TOK_UNDERSCORE; return TOK_UNDERSCORE; }

        "["    { state->last_token = TOK_LBRACKET; return TOK_LBRACKET; }
        "]"    { state->last_token = TOK_RBRACKET; return TOK_RBRACKET; }
        "("    { state->last_token = TOK_LPAREN; return TOK_LPAREN; }
        ")"    { state->last_token = TOK_RPAREN; return TOK_RPAREN; }
        "!"    { state->last_token = TOK_BANG; return TOK_BANG; }
        "|"    { state->last_token = TOK_PIPE; return TOK_PIPE; }
        "`"    { state->last_token = TOK_BACKTICK; return TOK_BACKTICK; }

        // Fenced code blocks
        [ ]{0,3} "```" [^ \n\r\t\x00]* [ ]* ("\n" | "\x00") {
            if (is_at_line_start) {
                text = std::string(tok_start, state->cursor - tok_start);
                state->last_token = TOK_CODE_FENCE;
                return TOK_CODE_FENCE;
            } else {
                text = "```";
                state->cursor = tok_start + 3;
                state->last_token = TOK_TEXT;
                return TOK_TEXT;
            }
        }

        "<br>" | "<br/>" { state->last_token = TOK_BR; return TOK_BR; }
        "<hr>" | "<hr/>" { state->last_token = TOK_HR_TAG; return TOK_HR_TAG; }
        "<u>"  { state->last_token = TOK_U_OPEN; return TOK_U_OPEN; }
        "</u>" { state->last_token = TOK_U_CLOSE; return TOK_U_CLOSE; }
        "&nbsp;" { state->last_token = TOK_NBSP; return TOK_NBSP; }

        "\\" [\\*#_~`!\[\]()|] { 
            text = std::string(tok_start + 1, 1);
            state->last_token = TOK_TEXT;
            return TOK_TEXT; 
        }

        // Refined TEXT rule: Avoid markers at the start
        [a-zA-Z,/:;?+@$%=^"'{}\. ] [a-zA-Z0-9 \t.,/:;?+@$%=^"'{}\-]* {
            text = std::string(tok_start, state->cursor - tok_start);
            state->last_token = TOK_TEXT;
            return TOK_TEXT;
        }
        
        [0-9]+ [^. \t\r\n] [a-zA-Z0-9 \t.,/:;?+@$%=^"'{}\-]* {
            text = std::string(tok_start, state->cursor - tok_start);
            state->last_token = TOK_TEXT;
            return TOK_TEXT;
        }
        
        [0-9]+ {
            text = std::string(tok_start, state->cursor - tok_start);
            state->last_token = TOK_TEXT;
            return TOK_TEXT;
        }

        . { 
            text = std::string(tok_start, 1);
            state->last_token = TOK_TEXT;
            return TOK_TEXT;
        }

        * { state->last_token = -1; return -1; }
    */
}

} // namespace md_re2c
