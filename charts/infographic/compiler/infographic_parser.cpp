// Infographic DSL Parser - Entry point
// Uses re2c generated lexer and lemon generated parser

#include <ir/unified_infographic.h>
#include "infographic_token.h"
#include "infographic_context.h"
#include <cstdio>
#include <cstdlib>
#include <string>

// Generated headers
#include "infographic_parser_gen.h"

// Lemon parser declarations
void* InfographicParseAlloc(void* (*mallocProc)(size_t));
void InfographicParseFree(void* p, void (*freeProc)(void*));
void InfographicParse(void* yyp, int yymajor, std::string* yyminor, 
                      flex::modules::infographic::ParseContext* ctx);

namespace flex::modules::infographic {

bool parse_infographic_dsl(const char* source, UnifiedInfographic* infographic, std::string& error_msg) {
    ParseContext ctx(infographic);
    void* parser = InfographicParseAlloc(malloc);
    LexerState* lexer = lexer_create(source);

    Token tok;
    do {
        tok = lex_next_token(lexer);

        if (tok.type == TOK_ERROR) {
            error_msg = "Lexer error: unrecognized character '" + tok.value + 
                       "' at line " + std::to_string(tok.line);
            break;
        }

        std::string* val = ctx.add_string(tok.value);
        InfographicParse(parser, tok.type, val, &ctx);
    } while (tok.type != TOK_EOF && ctx.error_message.empty());

    if (!ctx.error_message.empty()) {
        error_msg = ctx.error_message;
    }

    InfographicParseFree(parser, free);
    lexer_destroy(lexer);

    return ctx.error_message.empty() && tok.type != TOK_ERROR;
}

} // namespace flex::modules::infographic
