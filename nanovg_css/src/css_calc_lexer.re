// re2c --lang c
/*
 * CSS calc() Lexer - re2c scanner for CSS calc() expressions
 *
 * Tokenizes:
 * - Numbers with units (10px, 50%, 2em, 1.5rem, 100vw, 100vh)
 * - Operators (+, -, *, /)
 * - Parentheses
 * - Nested functions (min, max, clamp)
 *
 * Note: Sign handling (negative numbers) is done at parser level
 * since it's context-dependent (e.g., "10px - 5px" vs "10px + -5px")
 */

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Token types
typedef enum {
    CALC_END,
    CALC_NUMBER,        // Plain number (no unit)
    CALC_NUMBER_PX,     // Number with px unit
    CALC_NUMBER_PERCENT,// Number with % unit
    CALC_NUMBER_EM,     // Number with em unit
    CALC_NUMBER_REM,    // Number with rem unit
    CALC_NUMBER_VW,     // Number with vw unit
    CALC_NUMBER_VH,     // Number with vh unit
    CALC_NUMBER_DEG,    // Number with deg unit (for rotations)
    CALC_PLUS,          // +
    CALC_MINUS,         // -
    CALC_MULTIPLY,      // *
    CALC_DIVIDE,        // /
    CALC_LPAREN,        // (
    CALC_RPAREN,        // )
    CALC_COMMA,         // , (for min/max/clamp arguments)
    CALC_MIN,           // min function
    CALC_MAX,           // max function
    CALC_CLAMP,         // clamp function
    CALC_VAR,           // var() function (CSS variable)
    CALC_ERROR
} CSSCalcTokenType;

typedef struct {
    CSSCalcTokenType type;
    const char* start;   // Pointer to token start
    int length;          // Token length
    float value;         // Numeric value (for NUMBER tokens)
} CSSCalcToken;

typedef struct {
    const char* cursor;
    const char* marker;
    const char* limit;
    const char* token_start;
} CSSCalcLexer;

static void CSSCalcLexer_init(CSSCalcLexer* lexer, const char* input) {
    lexer->cursor = input;
    lexer->marker = input;
    lexer->limit = input + strlen(input);
    lexer->token_start = input;
}

static CSSCalcToken CSSCalcLexer_next_token(CSSCalcLexer* lexer) {
    CSSCalcToken token;
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

            // Numbers (including negative - sign handled separately for context)
            digit = [0-9];
            integer = digit+;
            fraction = "." digit+;
            number = (integer fraction? | fraction);

            // End of input
            "\x00" {
                token.type = CALC_END;
                token.start = lexer->token_start;
                token.length = 0;
                return token;
            }

            // Skip whitespace
            ws { continue; }

            // CSS Functions (must be before number patterns)
            "min" {
                token.type = CALC_MIN;
                token.start = lexer->token_start;
                token.length = 3;
                return token;
            }

            "max" {
                token.type = CALC_MAX;
                token.start = lexer->token_start;
                token.length = 3;
                return token;
            }

            "clamp" {
                token.type = CALC_CLAMP;
                token.start = lexer->token_start;
                token.length = 5;
                return token;
            }

            "var" {
                token.type = CALC_VAR;
                token.start = lexer->token_start;
                token.length = 3;
                return token;
            }

            // Numbers with units (longer units first to avoid partial matches)
            number "rem" {
                token.type = CALC_NUMBER_REM;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "deg" {
                token.type = CALC_NUMBER_DEG;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "px" {
                token.type = CALC_NUMBER_PX;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "em" {
                token.type = CALC_NUMBER_EM;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "vw" {
                token.type = CALC_NUMBER_VW;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "vh" {
                token.type = CALC_NUMBER_VH;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "%" {
                token.type = CALC_NUMBER_PERCENT;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            // Plain number (no unit)
            number {
                token.type = CALC_NUMBER;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            // Operators
            "+" {
                token.type = CALC_PLUS;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            "-" {
                token.type = CALC_MINUS;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            "*" {
                token.type = CALC_MULTIPLY;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            "/" {
                token.type = CALC_DIVIDE;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            // Parentheses and comma
            "(" {
                token.type = CALC_LPAREN;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            ")" {
                token.type = CALC_RPAREN;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            "," {
                token.type = CALC_COMMA;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            // Skip CSS variable names (--var-name)
            "--" [a-zA-Z_][a-zA-Z0-9_-]* {
                // CSS variable name - treat as a special token
                // Parser will need to resolve this via CSSVariableResolver
                token.type = CALC_NUMBER;  // Placeholder - will be resolved
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = 0.0f;
                return token;
            }

            // Anything else is an error
            * {
                token.type = CALC_ERROR;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }
        */
    }
}

// Helper: Check if token is a number type
static int CSSCalcToken_is_number(CSSCalcTokenType type) {
    return type == CALC_NUMBER ||
           type == CALC_NUMBER_PX ||
           type == CALC_NUMBER_PERCENT ||
           type == CALC_NUMBER_EM ||
           type == CALC_NUMBER_REM ||
           type == CALC_NUMBER_VW ||
           type == CALC_NUMBER_VH ||
           type == CALC_NUMBER_DEG;
}

// Helper: Resolve value to pixels given context
static float CSSCalcToken_to_pixels(CSSCalcToken* token, float context_value, float font_size, float viewport_width, float viewport_height) {
    switch (token->type) {
        case CALC_NUMBER:
        case CALC_NUMBER_PX:
            return token->value;
        case CALC_NUMBER_PERCENT:
            return (token->value / 100.0f) * context_value;
        case CALC_NUMBER_EM:
            return token->value * font_size;
        case CALC_NUMBER_REM:
            return token->value * 16.0f;  // Assume 16px root font
        case CALC_NUMBER_VW:
            return (token->value / 100.0f) * viewport_width;
        case CALC_NUMBER_VH:
            return (token->value / 100.0f) * viewport_height;
        case CALC_NUMBER_DEG:
            return token->value;  // Degrees don't convert to pixels
        default:
            return 0.0f;
    }
}

#ifdef __cplusplus
}
#endif
