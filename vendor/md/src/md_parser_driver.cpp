#include "md_re2c.h"
#include "md_parser_gen.h"
#include <iostream>

// Forward declarations from generated files (Compiled as C++ since they use std::string)
void* MDParseAlloc(void* (*mallocProc)(size_t));
void MDParse(void* parser, int tokenID, std::string* tokenData, md_re2c::ParseContext* ctx);
void MDParseFree(void* parser, void (*freeProc)(void*));

namespace md_re2c {

// Forward declaration for lex function (implemented in md_lexer_gen.cpp)
int lex(LexerState* state, std::string& text);

ParseResult parse(const std::string& input) {
    auto ctx_ptr = std::make_unique<ParseContext>();
    ParseContext& ctx = *ctx_ptr;

    // Ensure input ends with a newline so the grammar can close the last block
    std::string normalized_input = input;
    if (normalized_input.empty() || normalized_input.back() != '\n') {
        normalized_input += '\n';
    }

    LexerState state;
    state.start = normalized_input.c_str();
    state.cursor = normalized_input.c_str();
    state.marker = normalized_input.c_str();

    void* parser = MDParseAlloc(malloc);
    
    std::string text;
    int token;
    while ((token = lex(&state, text)) > 0) {
        std::string* data = new std::string(text);
        MDParse(parser, token, data, &ctx);
        text.clear();
    }
    
    // Sentinel for EOF
    MDParse(parser, 0, nullptr, &ctx);
    MDParseFree(parser, free);

    // Post-process blocks: merge paragraphs and identify tables
    if (ctx.root) {
        process_blocks(ctx.root, ctx.pool);
    }

    // Post-process inlines: resolve emphasis, links, etc.
    if (ctx.root) {
        process_inline_emphasis(ctx.root, ctx.pool);
    }

    return { std::move(ctx_ptr), ctx.root };
}

} // namespace md_re2c
