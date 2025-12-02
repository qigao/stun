// re2c --lang c
/*
 * SVG Path Lexer - re2c scanner for SVG path d attribute
 *
 * Generates tokens for SVG path commands and coordinates
 */

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Token types (C-compatible)
typedef enum {
    END,
    MOVETO_ABS,      // M
    MOVETO_REL,      // m
    LINETO_ABS,      // L
    LINETO_REL,      // l
    HLINETO_ABS,     // H
    HLINETO_REL,     // h
    VLINETO_ABS,     // V
    VLINETO_REL,     // v
    CURVETO_ABS,     // C
    CURVETO_REL,     // c
    SMOOTHCURVETO_ABS, // S
    SMOOTHCURVETO_REL, // s
    QUADTO_ABS,      // Q
    QUADTO_REL,      // q
    SMOOTHQUADTO_ABS, // T
    SMOOTHQUADTO_REL, // t
    ARC_ABS,         // A
    ARC_REL,         // a
    CLOSEPATH,       // Z or z
    NUMBER
} SVGPathTokenType;

typedef struct {
    SVGPathTokenType type;
    float value;  // For NUMBER tokens
} SVGPathToken;

typedef struct {
    const char* cursor;
    const char* marker;
    const char* limit;
} SVGPathLexer;

static void SVGPathLexer_init(SVGPathLexer* lexer, const char* input) {
    lexer->cursor = input;
    lexer->marker = input;
    lexer->limit = input + strlen(input);
}

static SVGPathToken SVGPathLexer_next_token(SVGPathLexer* lexer) {
    SVGPathToken token;

    while (1) {
        const char* YYMARKER = lexer->marker;
        const char* start = lexer->cursor;

        /*!re2c
            re2c:define:YYCTYPE = char;
            re2c:define:YYCURSOR = lexer->cursor;
            re2c:define:YYMARKER = lexer->marker;
            re2c:define:YYLIMIT = lexer->limit;
            re2c:yyfill:enable = 0;

            // Whitespace and separators
            ws = [ \t\r\n,]+;

            // Numbers (integer, float, scientific notation)
            sign = [+-];
            digit = [0-9];
            integer = sign? digit+;
            fraction = "." digit+;
            exponent = [eE] sign? digit+;
            number = sign? (digit+ fraction? | fraction) exponent?;

            // End of input
            "\x00" { token.type = END; return token; }

            // Skip whitespace
            ws { continue; }

            // Commands
            "M" { token.type = MOVETO_ABS; return token; }
            "m" { token.type = MOVETO_REL; return token; }
            "L" { token.type = LINETO_ABS; return token; }
            "l" { token.type = LINETO_REL; return token; }
            "H" { token.type = HLINETO_ABS; return token; }
            "h" { token.type = HLINETO_REL; return token; }
            "V" { token.type = VLINETO_ABS; return token; }
            "v" { token.type = VLINETO_REL; return token; }
            "C" { token.type = CURVETO_ABS; return token; }
            "c" { token.type = CURVETO_REL; return token; }
            "S" { token.type = SMOOTHCURVETO_ABS; return token; }
            "s" { token.type = SMOOTHCURVETO_REL; return token; }
            "Q" { token.type = QUADTO_ABS; return token; }
            "q" { token.type = QUADTO_REL; return token; }
            "T" { token.type = SMOOTHQUADTO_ABS; return token; }
            "t" { token.type = SMOOTHQUADTO_REL; return token; }
            "A" { token.type = ARC_ABS; return token; }
            "a" { token.type = ARC_REL; return token; }
            [Zz] { token.type = CLOSEPATH; return token; }

            // Numbers
            number {
                token.type = NUMBER;
                token.value = strtof(start, NULL);
                return token;
            }

            // Anything else is an error - skip it
            * { continue; }
        */
    }
}

#ifdef __cplusplus
}
#endif
