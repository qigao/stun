// re2c --lang c
#include "chart_token.h"
#include <cstdlib>
#include <cstring>

namespace flex {
namespace chart {

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
      ident = letter (letter | digit | [_] | [-])* ;
      hexdigit = [0-9a-fA-F];

      // End of file
      "\x00" {
          update_position(state, tok_start);
          return Token{TOK_EOF, "", state->line, state->column};
      }

      // Whitespace and comments (skip)
      ws { update_position(state, tok_start); goto yyloop; }
      comment { update_position(state, tok_start); goto yyloop; }

      // Root Keywords
      "title"      { update_position(state, tok_start); return Token{TOK_TITLE, "title", state->line, state->column}; }
      "width"      { update_position(state, tok_start); return Token{TOK_WIDTH, "width", state->line, state->column}; }
      "height"     { update_position(state, tok_start); return Token{TOK_HEIGHT, "height", state->line, state->column}; }
      "margin"     { update_position(state, tok_start); return Token{TOK_MARGIN, "margin", state->line, state->column}; }
      "theme"      { update_position(state, tok_start); return Token{TOK_THEME, "theme", state->line, state->column}; }
      "data"       { update_position(state, tok_start); return Token{TOK_DATA, "data", state->line, state->column}; }
      "signal"     { update_position(state, tok_start); return Token{TOK_SIGNAL, "signal", state->line, state->column}; }
      "scale"      { update_position(state, tok_start); return Token{TOK_SCALE, "scale", state->line, state->column}; }
      "axis"       { update_position(state, tok_start); return Token{TOK_AXIS, "axis", state->line, state->column}; }
      "legend"     { update_position(state, tok_start); return Token{TOK_LEGEND, "legend", state->line, state->column}; }
      "projection" { update_position(state, tok_start); return Token{TOK_PROJECTION, "projection", state->line, state->column}; }
      "value"      { update_position(state, tok_start); return Token{TOK_VALUE, "value", state->line, state->column}; }
      "on"         { update_position(state, tok_start); return Token{TOK_ON, "on", state->line, state->column}; }

      // Marks
      "bar"       { update_position(state, tok_start); return Token{TOK_BAR, "bar", state->line, state->column}; }
      "line"      { update_position(state, tok_start); return Token{TOK_LINE, "line", state->line, state->column}; }
      "arc"       { update_position(state, tok_start); return Token{TOK_ARC, "arc", state->line, state->column}; }
      "area"      { update_position(state, tok_start); return Token{TOK_AREA, "area", state->line, state->column}; }
      "point"     { update_position(state, tok_start); return Token{TOK_POINT, "point", state->line, state->column}; }
      "rect"      { update_position(state, tok_start); return Token{TOK_RECT, "rect", state->line, state->column}; }
      "rule"      { update_position(state, tok_start); return Token{TOK_RULE, "rule", state->line, state->column}; }
      "tick"      { update_position(state, tok_start); return Token{TOK_TICK, "tick", state->line, state->column}; }
      "text"      { update_position(state, tok_start); return Token{TOK_TEXT, "text", state->line, state->column}; }
      "trail"     { update_position(state, tok_start); return Token{TOK_TRAIL, "trail", state->line, state->column}; }
      "boxplot"   { update_position(state, tok_start); return Token{TOK_BOXPLOT, "boxplot", state->line, state->column}; }
      "errorbar"  { update_position(state, tok_start); return Token{TOK_ERRORBAR, "errorbar", state->line, state->column}; }
      "errorband" { update_position(state, tok_start); return Token{TOK_ERRORBAND, "errorband", state->line, state->column}; }
      "geoshape"  { update_position(state, tok_start); return Token{TOK_GEOSHAPE, "geoshape", state->line, state->column}; }
      "image"     { update_position(state, tok_start); return Token{TOK_IMAGE, "image", state->line, state->column}; }
      "pie"       { update_position(state, tok_start); return Token{TOK_PIE, "pie", state->line, state->column}; }
      "radar"     { update_position(state, tok_start); return Token{TOK_RADAR, "radar", state->line, state->column}; }

      // Composition
      "vconcat" { update_position(state, tok_start); return Token{TOK_VCONCAT, "vconcat", state->line, state->column}; }
      "hconcat" { update_position(state, tok_start); return Token{TOK_HCONCAT, "hconcat", state->line, state->column}; }
      "facet"   { update_position(state, tok_start); return Token{TOK_FACET, "facet", state->line, state->column}; }
      "repeat"  { update_position(state, tok_start); return Token{TOK_REPEAT, "repeat", state->line, state->column}; }

      // States
      "enter"  { update_position(state, tok_start); return Token{TOK_ENTER, "enter", state->line, state->column}; }
      "update" { update_position(state, tok_start); return Token{TOK_UPDATE, "update", state->line, state->column}; }
      "hover"  { update_position(state, tok_start); return Token{TOK_HOVER, "hover", state->line, state->column}; }
      "exit"   { update_position(state, tok_start); return Token{TOK_EXIT, "exit", state->line, state->column}; }

      // Attributes
      "source"    { update_position(state, tok_start); return Token{TOK_SOURCE, "source", state->line, state->column}; }
      "format"    { update_position(state, tok_start); return Token{TOK_FORMAT, "format", state->line, state->column}; }
      "values"    { update_position(state, tok_start); return Token{TOK_VALUES, "values", state->line, state->column}; }
      "transform" { update_position(state, tok_start); return Token{TOK_TRANSFORM, "transform", state->line, state->column}; }
      "encoding"  { update_position(state, tok_start); return Token{TOK_ENCODING, "encoding", state->line, state->column}; }
      "spec"      { update_position(state, tok_start); return Token{TOK_SPEC, "spec", state->line, state->column}; }
      "resolve"   { update_position(state, tok_start); return Token{TOK_RESOLVE, "resolve", state->line, state->column}; }

      // Literals
      "true" { update_position(state, tok_start); return Token{TOK_BOOL, "true", state->line, state->column}; }
      "false" { update_position(state, tok_start); return Token{TOK_BOOL, "false", state->line, state->column}; }

      // Symbols
      "{" { update_position(state, tok_start); return Token{TOK_LBRACE, "{", state->line, state->column}; }
      "}" { update_position(state, tok_start); return Token{TOK_RBRACE, "}", state->line, state->column}; }
      "[" { update_position(state, tok_start); return Token{TOK_LBRACKET, "[", state->line, state->column}; }
      "]" { update_position(state, tok_start); return Token{TOK_RBRACKET, "]", state->line, state->column}; }
      "(" { update_position(state, tok_start); return Token{TOK_LPAREN, "(", state->line, state->column}; }
      ")" { update_position(state, tok_start); return Token{TOK_RPAREN, ")", state->line, state->column}; }
      ":" { update_position(state, tok_start); return Token{TOK_COLON, ":", state->line, state->column}; }
      "," { update_position(state, tok_start); return Token{TOK_COMMA, ",", state->line, state->column}; }
      "." { update_position(state, tok_start); return Token{TOK_DOT, ".", state->line, state->column}; }
      "=" { update_position(state, tok_start); return Token{TOK_ASSIGN, "=", state->line, state->column}; }
      "?" { update_position(state, tok_start); return Token{TOK_QUESTION, "?", state->line, state->column}; }

      // String literal
      "\"" ([^"\\] | "\\" [^\x00])* "\"" {
          update_position(state, tok_start);
          std::string value(tok_start + 1, state->cursor - tok_start - 2);
          return Token{TOK_STRING, value, state->line, state->column};
      }

      // Percentage (e.g., 50%)
      "-"? digit+ ("." digit+)? "%" {
          update_position(state, tok_start);
          std::string value(tok_start, state->cursor - tok_start);
          return Token{TOK_PERCENTAGE, value, state->line, state->column};
      }

      // Number (includes pixels like 800px)
      "-"? digit+ ("." digit+)? ([pP][xX])? {
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

      // Identifier
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

} // namespace chart
} // namespace flex
