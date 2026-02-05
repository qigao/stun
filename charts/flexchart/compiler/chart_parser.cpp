#include "flexchart/chart_ast.h"
#include "chart_token.h"
#include "chart_context.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// Generated headers
#include "chart_parser_gen.h"

// Lemon parser declarations (Global scope to match Lemon output)
void *ChartParseAlloc(void *(*mallocProc)(size_t));
void ChartParseFree(void *p, void (*freeProc)(void *));
void ChartParse(void *yyp, int yymajor, std::string* yyminor, flex::chart::ParseContext *ctx);

namespace flex {
namespace chart {

bool parse_chart(const char *source, AstProgram *program, std::string &error_msg) {
    ParseContext ctx(program);
    void *parser = ChartParseAlloc(malloc);
    LexerState *lexer = lexer_create(source);
    
    Token tok;
    do {
        tok = lex_next_token(lexer);
        
        if (tok.type == TOK_ERROR) {
            error_msg = "Lexer error: unrecognized character '" + tok.value + "' at line " + std::to_string(tok.line);
            break;
        }

        std::string* val = ctx.add_string(tok.value);
        ChartParse(parser, tok.type, val, &ctx);
    } while (tok.type != TOK_EOF && ctx.error_message.empty());
    
    if (!ctx.error_message.empty()) {
        error_msg = ctx.error_message;
    }
    
    ChartParseFree(parser, free);
    lexer_destroy(lexer);
    
    return ctx.error_message.empty() && tok.type != TOK_ERROR;
}

} // namespace chart
} // namespace flex
