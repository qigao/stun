// re2c lexer for infographic DSL
#include "infographic_token.h"
#include <cstdlib>
#include <cstring>

namespace flex::modules::infographic {

static void update_position(LexerState* state, const char* from) {
    while (from < state->cursor) {
        if (*from == '\n') {
            state->line++;
            state->column = 1;
        } else {
            state->column++;
        }
        from++;
    }
}

Token lex_next_token(LexerState* state) {
    const char* tok_start = state->cursor;

yyloop:
    tok_start = state->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = state->cursor;
        re2c:define:YYMARKER = state->marker;
        re2c:yyfill:enable = 0;

        // Helpers
        ws = [ \t\r\n]+;
        comment = "//" [^\n]*;
        digit = [0-9];
        letter = [a-zA-Z_];
        ident = letter (letter | digit | [_])* ;
        template_name = letter (letter | digit | [_])* [-] (letter | digit | [_] | [-])+ ;
        hexdigit = [0-9a-fA-F];

        // End of file
        "\x00" {
            update_position(state, tok_start);
            return Token{TOK_EOF, "", state->line, state->column};
        }

        // Whitespace and comments (skip)
        ws { update_position(state, tok_start); goto yyloop; }
        comment { update_position(state, tok_start); goto yyloop; }

        // Keywords
        "infographic" { update_position(state, tok_start); return Token{TOK_INFOGRAPHIC, "infographic", state->line, state->column}; }
        "data"        { update_position(state, tok_start); return Token{TOK_DATA, "data", state->line, state->column}; }
        "theme"       { update_position(state, tok_start); return Token{TOK_THEME, "theme", state->line, state->column}; }
        "items"       { update_position(state, tok_start); return Token{TOK_ITEMS, "items", state->line, state->column}; }
        "children"    { update_position(state, tok_start); return Token{TOK_CHILDREN, "children", state->line, state->column}; }

        // Data fields
        "title"       { update_position(state, tok_start); return Token{TOK_TITLE, "title", state->line, state->column}; }
        "desc"        { update_position(state, tok_start); return Token{TOK_DESC, "desc", state->line, state->column}; }
        "label"       { update_position(state, tok_start); return Token{TOK_LABEL, "label", state->line, state->column}; }
        "value"       { update_position(state, tok_start); return Token{TOK_VALUE, "value", state->line, state->column}; }
        "icon"        { update_position(state, tok_start); return Token{TOK_ICON, "icon", state->line, state->column}; }
        "illus"       { update_position(state, tok_start); return Token{TOK_ILLUS, "illus", state->line, state->column}; }
        "done"        { update_position(state, tok_start); return Token{TOK_DONE, "done", state->line, state->column}; }

        // Theme fields
        "palette"     { update_position(state, tok_start); return Token{TOK_PALETTE, "palette", state->line, state->column}; }
        "preset"      { update_position(state, tok_start); return Token{TOK_PRESET, "preset", state->line, state->column}; }
        "stylize"     { update_position(state, tok_start); return Token{TOK_STYLIZE, "stylize", state->line, state->column}; }

        // Boolean literals
        "true"  { update_position(state, tok_start); return Token{TOK_BOOL, "true", state->line, state->column}; }
        "false" { update_position(state, tok_start); return Token{TOK_BOOL, "false", state->line, state->column}; }

        // Symbols
        "{" { update_position(state, tok_start); return Token{TOK_LBRACE, "{", state->line, state->column}; }
        "}" { update_position(state, tok_start); return Token{TOK_RBRACE, "}", state->line, state->column}; }
        "[" { update_position(state, tok_start); return Token{TOK_LBRACKET, "[", state->line, state->column}; }
        "]" { update_position(state, tok_start); return Token{TOK_RBRACKET, "]", state->line, state->column}; }
        "(" { update_position(state, tok_start); return Token{TOK_LPAREN, "(", state->line, state->column}; }
        ")" { update_position(state, tok_start); return Token{TOK_RPAREN, ")", state->line, state->column}; }
        "+" { update_position(state, tok_start); return Token{TOK_PLUS, "+", state->line, state->column}; }
        "-" { update_position(state, tok_start); return Token{TOK_MINUS, "-", state->line, state->column}; }
        "*" { update_position(state, tok_start); return Token{TOK_STAR, "*", state->line, state->column}; }
        "/" { update_position(state, tok_start); return Token{TOK_SLASH, "/", state->line, state->column}; }
        "%" { update_position(state, tok_start); return Token{TOK_PERCENT, "%", state->line, state->column}; }
        ":" { update_position(state, tok_start); return Token{TOK_COLON, ":", state->line, state->column}; }
        "," { update_position(state, tok_start); return Token{TOK_COMMA, ",", state->line, state->column}; }

        // String literal
        "\"" ([^"\\] | "\\" [^\x00])* "\"" {
            update_position(state, tok_start);
            std::string value(tok_start + 1, state->cursor - tok_start - 2);
            return Token{TOK_STRING, value, state->line, state->column};
        }

        // Number
        digit+ ("." digit+)? {
            update_position(state, tok_start);
            std::string value(tok_start, state->cursor - tok_start);
            return Token{TOK_NUMBER, value, state->line, state->column};
        }

        // Color (hex)
        "#" hexdigit+ {
            update_position(state, tok_start);
            std::string value(tok_start, state->cursor - tok_start);
            return Token{TOK_COLOR, value, state->line, state->column};
        }

        // Template name (with dashes, e.g., list-grid-badge-card)
        template_name {
            update_position(state, tok_start);
            std::string value(tok_start, state->cursor - tok_start);
            return Token{TOK_TEMPLATE_NAME, value, state->line, state->column};
        }

        // Identifier (without dashes)
        ident {
            update_position(state, tok_start);
            std::string value(tok_start, state->cursor - tok_start);
            return Token{TOK_IDENTIFIER, value, state->line, state->column};
        }

        // Error
        * {
            update_position(state, tok_start);
            return Token{TOK_ERROR, std::string(1, *tok_start), state->line, state->column};
        }
    */
}

LexerState* lexer_create(const char* source) {
    auto* state = new LexerState;
    state->start = source;
    state->cursor = source;
    state->marker = source;
    state->line = 1;
    state->column = 1;
    return state;
}

void lexer_destroy(LexerState* state) {
    delete state;
}

} // namespace flex::modules::infographic
