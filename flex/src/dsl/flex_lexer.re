// re2c --lang c
/*
 * Flex DSL Lexer - re2c
 * Tokenizes .flex files for the parser
 */

#include "flex/dsl/flex_token.h"
#include <cctype>
#include <cstdlib>
#include <cstring>


namespace flex {
namespace parser {

static void update_position(LexerState *state, const char *from) {
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

static bool is_ident_continue(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

static bool is_hex_color_length(size_t count) {
  return count == 3 || count == 4 || count == 6 || count == 8;
}

Token lex_next_token(LexerState *state) {
  const char *tok_start = state->cursor;

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
      ident = letter (letter | digit | [_])*;
      hexdigit = [0-9a-fA-F];

      // End of file
      "\x00" {
          update_position(state, tok_start);
          return Token{TOK_EOF, "", state->line, state->column};
      }

      // Whitespace and comments (skip, but update position for line tracking)
      ws { update_position(state, tok_start); goto yyloop; }
      comment { update_position(state, tok_start); goto yyloop; }

      // Keywords - Top-level blocks
      "import" {
          update_position(state, tok_start);
          return Token{TOK_IMPORT, "import", state->line, state->column};
      }

      "scene" {
          update_position(state, tok_start);
          return Token{TOK_SCENE, "scene", state->line, state->column};
      }

      "artboard" {
          update_position(state, tok_start);
          return Token{TOK_ARTBOARD, "artboard", state->line, state->column};
      }

      "component" {
          update_position(state, tok_start);
          return Token{TOK_COMPONENT, "component", state->line, state->column};
      }

      "anim" {
          update_position(state, tok_start);
          return Token{TOK_ANIM, "anim", state->line, state->column};
      }

      "machine" {
          update_position(state, tok_start);
          return Token{TOK_MACHINE, "machine", state->line, state->column};
      }

      // Machine keywords
      "layer" {
          update_position(state, tok_start);
          return Token{TOK_LAYER, "layer", state->line, state->column};
      }

      "state" {
          update_position(state, tok_start);
          return Token{TOK_STATE, "state", state->line, state->column};
      }

      "transition" {
          update_position(state, tok_start);
          return Token{TOK_TRANSITION, "transition", state->line, state->column};
      }

      "when" {
          update_position(state, tok_start);
          return Token{TOK_WHEN, "when", state->line, state->column};
      }

      "repeat" {
          update_position(state, tok_start);
          return Token{TOK_REPEAT, "repeat", state->line, state->column};
      }

      "data" {
          update_position(state, tok_start);
          return Token{TOK_DATA, "data", state->line, state->column};
      }

      "assets" {
          update_position(state, tok_start);
          return Token{TOK_ASSETS, "assets", state->line, state->column};
      }

      "audio" {
          update_position(state, tok_start);
          return Token{TOK_AUDIO, "audio", state->line, state->column};
      }

      "font" {
          update_position(state, tok_start);
          return Token{TOK_FONT, "font", state->line, state->column};
      }

      "for" {
          update_position(state, tok_start);
          return Token{TOK_FOR, "for", state->line, state->column};
      }

      "in" {
          update_position(state, tok_start);
          return Token{TOK_IN, "in", state->line, state->column};
      }

      "const" {
          update_position(state, tok_start);
          return Token{TOK_CONST, "const", state->line, state->column};
      }

      "var" {
          update_position(state, tok_start);
          return Token{TOK_VAR, "var", state->line, state->column};
      }

      "set" {
          update_position(state, tok_start);
          return Token{TOK_SET, "set", state->line, state->column};
      }

      "play" {
          update_position(state, tok_start);
          return Token{TOK_PLAY, "play", state->line, state->column};
      }

      "with" {
          update_position(state, tok_start);
          return Token{TOK_WITH, "with", state->line, state->column};
      }

      // Animation keywords
      "track" {
          update_position(state, tok_start);
          return Token{TOK_TRACK, "track", state->line, state->column};
      }

      "keyframe" {
          update_position(state, tok_start);
          return Token{TOK_KEYFRAME, "keyframe", state->line, state->column};
      }

      // Node types
      "group" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "group", state->line, state->column};
      }

      "rect" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "rect", state->line, state->column};
      }

      "circle" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "circle", state->line, state->column};
      }

      "ellipse" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "ellipse", state->line, state->column};
      }

      "polygon" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "polygon", state->line, state->column};
      }

      "star" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "star", state->line, state->column};
      }

      "text" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "text", state->line, state->column};
      }

      "image" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "image", state->line, state->column};
      }

      "img" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "img", state->line, state->column};
      }

      "svg" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "svg", state->line, state->column};
      }

      "path" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "path", state->line, state->column};
      }

      "line" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "line", state->line, state->column};
      }

      "ring" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "ring", state->line, state->column};
      }

      "triangle" {
          update_position(state, tok_start);
          return Token{TOK_NODE_TYPE, "triangle", state->line, state->column};
      }

      // Boolean literals
      "true" {
          update_position(state, tok_start);
          return Token{TOK_BOOL, "true", state->line, state->column};
      }

      "false" {
          update_position(state, tok_start);
          return Token{TOK_BOOL, "false", state->line, state->column};
      }

      // Operators and symbols
      "->" {
          update_position(state, tok_start);
          return Token{TOK_ARROW, "->", state->line, state->column};
      }

      "*" {
          update_position(state, tok_start);
          return Token{TOK_STAR, "*", state->line, state->column};
      }

      "+" {
          update_position(state, tok_start);
          return Token{TOK_PLUS, "+", state->line, state->column};
      }

      "-" {
          update_position(state, tok_start);
          return Token{TOK_MINUS, "-", state->line, state->column};
      }

      "/" {
          update_position(state, tok_start);
          return Token{TOK_SLASH, "/", state->line, state->column};
      }

      "(" {
          update_position(state, tok_start);
          return Token{TOK_LPAREN, "(", state->line, state->column};
      }

      ")" {
          update_position(state, tok_start);
          return Token{TOK_RPAREN, ")", state->line, state->column};
      }


      ">" {
          update_position(state, tok_start);
          return Token{TOK_GT, ">", state->line, state->column};
      }

      "<" {
          update_position(state, tok_start);
          return Token{TOK_LT, "<", state->line, state->column};
      }

      "==" {
          update_position(state, tok_start);
          return Token{TOK_EQ, "==", state->line, state->column};
      }

      "!=" {
          update_position(state, tok_start);
          return Token{TOK_NEQ, "!=", state->line, state->column};
      }

      "=" {
          update_position(state, tok_start);
          return Token{TOK_ASSIGN, "=", state->line, state->column};
      }

      "{" {
          update_position(state, tok_start);
          return Token{TOK_LBRACE, "{", state->line, state->column};
      }

      "}" {
          update_position(state, tok_start);
          return Token{TOK_RBRACE, "}", state->line, state->column};
      }

      ":" {
          update_position(state, tok_start);
          return Token{TOK_COLON, ":", state->line, state->column};
      }

      "," {
          update_position(state, tok_start);
          return Token{TOK_COMMA, ",", state->line, state->column};
      }

      // Binding expression: ${pos.x}
      "${" [^}\n]+ "}" {
          update_position(state, tok_start);
          std::string raw(tok_start, state->cursor - tok_start);
          // Store as ${...} format
          return Token{TOK_BINDING, raw, state->line, state->column};
      }

      // Binding expression: $(pos.x)
      "$(" [^)\n]+ ")" {
          update_position(state, tok_start);
          std::string raw(tok_start, state->cursor - tok_start);
          return Token{TOK_BINDING, raw, state->line, state->column};
      }

      // Simple binding: $name
      "$" ident {
          update_position(state, tok_start);
          std::string raw(tok_start, state->cursor - tok_start);
          return Token{TOK_BINDING, raw, state->line, state->column};
      }

      "$" {
          update_position(state, tok_start);
          return Token{TOK_DOLLAR, "$", state->line, state->column};
      }

      "@" {
          update_position(state, tok_start);
          return Token{TOK_AT, "@", state->line, state->column};
      }

      "#" {
          const char *scan = state->cursor;
          while (std::isxdigit(static_cast<unsigned char>(*scan)) != 0) {
              ++scan;
          }
          size_t hex_count = static_cast<size_t>(scan - state->cursor);
          if (is_hex_color_length(hex_count) &&
              !is_ident_continue(*scan) &&
              *scan != '.') {
              state->cursor = scan;
              update_position(state, tok_start);
              std::string value(tok_start, state->cursor - tok_start);
              return Token{TOK_COLOR, value, state->line, state->column};
          }
          update_position(state, tok_start);
          return Token{TOK_HASH, "#", state->line, state->column};
      }

      "/" {
          update_position(state, tok_start);
          return Token{TOK_SLASH, "/", state->line, state->column};
      }

      "." {
          update_position(state, tok_start);
          return Token{TOK_DOT, ".", state->line, state->column};
      }

      // String literal (double-quoted)
      "\"" ([^"\\] | "\\" [^\x00])* "\"" {
          update_position(state, tok_start);
          // Remove quotes
          std::string value(tok_start + 1, state->cursor - tok_start - 2);
          return Token{TOK_STRING, value, state->line, state->column};
      }

      // Number (integer or float, with optional sign and time unit)
      "-"? digit+ ("." digit+)? ([sS] | [mM][sS])? {
          update_position(state, tok_start);
          std::string value(tok_start, state->cursor - tok_start);
          return Token{TOK_NUMBER, value, state->line, state->column};
      }

      // Identifier (includes pseudo-class with leading :)
      ":" ident {
          update_position(state, tok_start);
          std::string value(tok_start, state->cursor - tok_start);
          return Token{TOK_IDENTIFIER, value, state->line, state->column};
      }

      ident {
          update_position(state, tok_start);
          std::string value(tok_start, state->cursor - tok_start);
          return Token{TOK_IDENTIFIER, value, state->line, state->column};
      }

      // Error: unrecognized character
      * {
          update_position(state, tok_start);
          char c = *tok_start;
          return Token{TOK_ERROR, std::string(1, c), state->line, state->column};
      }
  */
}

LexerState *lexer_create(const char *source) {
  auto *state = new LexerState;
  state->start = source;
  state->cursor = source;
  state->marker = source;
  state->line = 1;
  state->column = 1;
  return state;
}

void lexer_destroy(LexerState *state) { delete state; }

} // namespace parser
} // namespace flex
