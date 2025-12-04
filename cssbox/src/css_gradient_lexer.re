// re2c --lang c
/*
 * CSS Gradient Lexer - re2c scanner for CSS gradient syntax
 *
 * Tokenizes:
 * - linear-gradient(direction, color-stop, ...)
 * - radial-gradient(shape size at position, color-stop, ...)
 *
 * Token types match CSS gradient grammar
 */

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Token types
typedef enum {
    GRAD_END,
    GRAD_LINEAR,         // "linear-gradient"
    GRAD_RADIAL,         // "radial-gradient"
    GRAD_REPEATING_LINEAR, // "repeating-linear-gradient"
    GRAD_REPEATING_RADIAL, // "repeating-radial-gradient"
    GRAD_TO,             // "to"
    GRAD_TOP,            // "top"
    GRAD_BOTTOM,         // "bottom"
    GRAD_LEFT,           // "left"
    GRAD_RIGHT,          // "right"
    GRAD_CENTER,         // "center"
    GRAD_AT,             // "at"
    GRAD_CIRCLE,         // "circle"
    GRAD_ELLIPSE,        // "ellipse"
    GRAD_CLOSEST_SIDE,   // "closest-side"
    GRAD_FARTHEST_SIDE,  // "farthest-side"
    GRAD_CLOSEST_CORNER, // "closest-corner"
    GRAD_FARTHEST_CORNER,// "farthest-corner"
    GRAD_NUMBER,         // numeric value
    GRAD_PERCENT,        // %
    GRAD_PX,             // px
    GRAD_DEG,            // deg
    GRAD_RAD,            // rad
    GRAD_TURN,           // turn
    GRAD_HEX_COLOR,      // #rgb or #rrggbb
    GRAD_RGB,            // "rgb"
    GRAD_RGBA,           // "rgba"
    GRAD_HSL,            // "hsl"
    GRAD_HSLA,           // "hsla"
    GRAD_NAMED_COLOR,    // named color (red, blue, etc.)
    GRAD_LPAREN,         // (
    GRAD_RPAREN,         // )
    GRAD_COMMA,          // ,
    GRAD_ERROR
} CSSGradientTokenType;

typedef struct {
    CSSGradientTokenType type;
    const char* start;   // Pointer to token start
    int length;          // Token length
    float value;         // For NUMBER tokens
} CSSGradientToken;

typedef struct {
    const char* cursor;
    const char* marker;
    const char* limit;
    const char* token_start;
} CSSGradientLexer;

static void CSSGradientLexer_init(CSSGradientLexer* lexer, const char* input) {
    lexer->cursor = input;
    lexer->marker = input;
    lexer->limit = input + strlen(input);
    lexer->token_start = input;
}

static CSSGradientToken CSSGradientLexer_next_token(CSSGradientLexer* lexer) {
    CSSGradientToken token;
    token.value = 0.0f;

    while (1) {
        const char* YYMARKER = lexer->marker;
        lexer->token_start = lexer->cursor;

        /*!re2c
            re2c:define:YYCTYPE = char;
            re2c:define:YYCURSOR = lexer->cursor;
            re2c:define:YYMARKER = lexer->marker;
            re2c:define:YYLIMIT = lexer->limit;
            re2c:yyfill:enable = 0;

            // Whitespace
            ws = [ \t\r\n]+;

            // Numbers
            sign = [+-];
            digit = [0-9];
            integer = digit+;
            fraction = "." digit+;
            number = sign? (integer fraction? | fraction);

            // Hex color
            hexdigit = [0-9a-fA-F];
            hex3 = "#" hexdigit{3};
            hex6 = "#" hexdigit{6};
            hex8 = "#" hexdigit{8};
            hexcolor = hex3 | hex6 | hex8;

            // Named colors (common subset)
            named_color = "transparent" | "black" | "white" | "red" | "green" | "blue" |
                          "yellow" | "cyan" | "magenta" | "gray" | "grey" | "orange" |
                          "purple" | "brown" | "pink" | "lime" | "navy" | "teal" |
                          "olive" | "maroon" | "aqua" | "silver" | "fuchsia" |
                          "aliceblue" | "antiquewhite" | "beige" | "coral" | "crimson" |
                          "darkblue" | "darkgray" | "darkgreen" | "darkred" | "gold" |
                          "indigo" | "ivory" | "khaki" | "lavender" | "lightblue" |
                          "lightgray" | "lightgreen" | "lightyellow" | "salmon" | "tan" |
                          "tomato" | "turquoise" | "violet" | "wheat" | "whitesmoke";

            // End of input
            "\x00" {
                token.type = GRAD_END;
                token.start = lexer->token_start;
                token.length = 0;
                return token;
            }

            // Skip whitespace
            ws { continue; }

            // Gradient function names
            "linear-gradient" {
                token.type = GRAD_LINEAR;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                return token;
            }

            "radial-gradient" {
                token.type = GRAD_RADIAL;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                return token;
            }

            "repeating-linear-gradient" {
                token.type = GRAD_REPEATING_LINEAR;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                return token;
            }

            "repeating-radial-gradient" {
                token.type = GRAD_REPEATING_RADIAL;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                return token;
            }

            // Direction keywords
            "to"      { token.type = GRAD_TO; token.start = lexer->token_start; token.length = 2; return token; }
            "top"     { token.type = GRAD_TOP; token.start = lexer->token_start; token.length = 3; return token; }
            "bottom"  { token.type = GRAD_BOTTOM; token.start = lexer->token_start; token.length = 6; return token; }
            "left"    { token.type = GRAD_LEFT; token.start = lexer->token_start; token.length = 4; return token; }
            "right"   { token.type = GRAD_RIGHT; token.start = lexer->token_start; token.length = 5; return token; }
            "center"  { token.type = GRAD_CENTER; token.start = lexer->token_start; token.length = 6; return token; }
            "at"      { token.type = GRAD_AT; token.start = lexer->token_start; token.length = 2; return token; }

            // Radial shape keywords
            "circle"          { token.type = GRAD_CIRCLE; token.start = lexer->token_start; token.length = 6; return token; }
            "ellipse"         { token.type = GRAD_ELLIPSE; token.start = lexer->token_start; token.length = 7; return token; }
            "closest-side"    { token.type = GRAD_CLOSEST_SIDE; token.start = lexer->token_start; token.length = 12; return token; }
            "farthest-side"   { token.type = GRAD_FARTHEST_SIDE; token.start = lexer->token_start; token.length = 13; return token; }
            "closest-corner"  { token.type = GRAD_CLOSEST_CORNER; token.start = lexer->token_start; token.length = 14; return token; }
            "farthest-corner" { token.type = GRAD_FARTHEST_CORNER; token.start = lexer->token_start; token.length = 15; return token; }

            // Color functions
            "rgba" { token.type = GRAD_RGBA; token.start = lexer->token_start; token.length = 4; return token; }
            "rgb"  { token.type = GRAD_RGB; token.start = lexer->token_start; token.length = 3; return token; }
            "hsla" { token.type = GRAD_HSLA; token.start = lexer->token_start; token.length = 4; return token; }
            "hsl"  { token.type = GRAD_HSL; token.start = lexer->token_start; token.length = 3; return token; }

            // Hex colors
            hexcolor {
                token.type = GRAD_HEX_COLOR;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                return token;
            }

            // Named colors
            named_color {
                token.type = GRAD_NAMED_COLOR;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                return token;
            }

            // Numbers with units (must be before plain number)
            number "%" {
                token.type = GRAD_PERCENT;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "px" {
                token.type = GRAD_PX;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "deg" {
                token.type = GRAD_DEG;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "rad" {
                token.type = GRAD_RAD;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "turn" {
                token.type = GRAD_TURN;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            // Plain number
            number {
                token.type = GRAD_NUMBER;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            // Punctuation
            "(" { token.type = GRAD_LPAREN; token.start = lexer->token_start; token.length = 1; return token; }
            ")" { token.type = GRAD_RPAREN; token.start = lexer->token_start; token.length = 1; return token; }
            "," { token.type = GRAD_COMMA; token.start = lexer->token_start; token.length = 1; return token; }

            // Anything else is an error
            * {
                token.type = GRAD_ERROR;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }
        */
    }
}

#ifdef __cplusplus
}
#endif
