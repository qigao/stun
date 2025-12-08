// re2c --lang c
/*
 * CSS Grid Track Lexer - re2c scanner for grid-template-columns/rows
 *
 * Tokenizes:
 * - Numbers with units (100px, 1fr, 50%)
 * - Keywords (auto, min-content, max-content)
 * - Functions (minmax, fit-content, repeat)
 * - Auto keywords (auto-fill, auto-fit)
 * - Parentheses and commas
 */

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Token types
typedef enum {
    GRID_END,
    GRID_NUMBER_PX,      // 100px
    GRID_NUMBER_FR,      // 1fr
    GRID_NUMBER_PERCENT, // 50%
    GRID_NUMBER,         // plain number (for repeat count)
    GRID_AUTO,           // auto
    GRID_MIN_CONTENT,    // min-content
    GRID_MAX_CONTENT,    // max-content
    GRID_MINMAX,         // minmax function
    GRID_FIT_CONTENT,    // fit-content function
    GRID_REPEAT,         // repeat function
    GRID_AUTO_FILL,      // auto-fill (inside repeat)
    GRID_AUTO_FIT,       // auto-fit (inside repeat)
    GRID_LPAREN,         // (
    GRID_RPAREN,         // )
    GRID_COMMA,          // ,
    GRID_LINE_NAME,      // [name] or [name1 name2]
    GRID_ERROR
} CSSGridTokenType;

typedef struct {
    CSSGridTokenType type;
    const char* start;   // Pointer to token start
    int length;          // Token length
    float value;         // Numeric value (for NUMBER tokens)
} CSSGridToken;

typedef struct {
    const char* cursor;
    const char* marker;
    const char* limit;
    const char* token_start;
} CSSGridLexer;

static void CSSGridLexer_init(CSSGridLexer* lexer, const char* input) {
    lexer->cursor = input;
    lexer->marker = input;
    lexer->limit = input + strlen(input);
    lexer->token_start = input;
}

static CSSGridToken CSSGridLexer_next_token(CSSGridLexer* lexer) {
    CSSGridToken token;
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
            digit = [0-9];
            integer = digit+;
            fraction = "." digit+;
            number = (integer fraction? | fraction);

            // End of input
            "\x00" {
                token.type = GRID_END;
                token.start = lexer->token_start;
                token.length = 0;
                return token;
            }

            // Skip whitespace
            ws { continue; }

            // Keywords (must be before number patterns)
            // Longer keywords first to avoid partial matches
            "auto-fill" {
                token.type = GRID_AUTO_FILL;
                token.start = lexer->token_start;
                token.length = 9;
                return token;
            }

            "auto-fit" {
                token.type = GRID_AUTO_FIT;
                token.start = lexer->token_start;
                token.length = 8;
                return token;
            }

            "min-content" {
                token.type = GRID_MIN_CONTENT;
                token.start = lexer->token_start;
                token.length = 11;
                return token;
            }

            "max-content" {
                token.type = GRID_MAX_CONTENT;
                token.start = lexer->token_start;
                token.length = 11;
                return token;
            }

            "fit-content" {
                token.type = GRID_FIT_CONTENT;
                token.start = lexer->token_start;
                token.length = 11;
                return token;
            }

            "minmax" {
                token.type = GRID_MINMAX;
                token.start = lexer->token_start;
                token.length = 6;
                return token;
            }

            "repeat" {
                token.type = GRID_REPEAT;
                token.start = lexer->token_start;
                token.length = 6;
                return token;
            }

            "auto" {
                token.type = GRID_AUTO;
                token.start = lexer->token_start;
                token.length = 4;
                return token;
            }

            // Numbers with units (longer units first)
            number "px" {
                token.type = GRID_NUMBER_PX;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "fr" {
                token.type = GRID_NUMBER_FR;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            number "%" {
                token.type = GRID_NUMBER_PERCENT;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            // Plain number (for repeat count)
            number {
                token.type = GRID_NUMBER;
                token.start = lexer->token_start;
                token.length = (int)(lexer->cursor - lexer->token_start);
                token.value = strtof(lexer->token_start, NULL);
                return token;
            }

            // Parentheses and comma
            "(" {
                token.type = GRID_LPAREN;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            ")" {
                token.type = GRID_RPAREN;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            "," {
                token.type = GRID_COMMA;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }

            // Named grid lines: [name] or [name1 name2]
            // Matches everything between [ and ]
            "[" [^\]]+ "]" {
                token.type = GRID_LINE_NAME;
                token.start = lexer->token_start + 1;  // Skip opening [
                token.length = (int)(lexer->cursor - lexer->token_start - 2);  // Exclude [ and ]
                return token;
            }

            // Anything else is an error
            * {
                token.type = GRID_ERROR;
                token.start = lexer->token_start;
                token.length = 1;
                return token;
            }
        */
    }
}

// Helper: Check if token is a size value
static int CSSGridToken_is_size(CSSGridTokenType type) {
    return type == GRID_NUMBER_PX ||
           type == GRID_NUMBER_FR ||
           type == GRID_NUMBER_PERCENT ||
           type == GRID_AUTO ||
           type == GRID_MIN_CONTENT ||
           type == GRID_MAX_CONTENT;
}

#ifdef __cplusplus
}
#endif
