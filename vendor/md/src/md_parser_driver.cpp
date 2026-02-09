#include "md.h"
#include "md_block_parser.h"
#include "md_inline_parser.h"
#include "md_extension.h"

namespace md {

ParseResult parse(const std::string& input) {
    auto ctx_ptr = std::make_unique<ParseContext>();
    
    // 阶段1: Block 解析
    BlockParser block_parser;
    auto blocks = block_parser.parse(input);
    
    // 阶段2: Inline 解析 (Block → Node AST)
    ctx_ptr->root = blocks_to_ast(*blocks, ctx_ptr->pool);
    
    // 阶段3: 扩展处理 (图表等)
    if (ctx_ptr->root) {
        process_blocks(ctx_ptr->root, ctx_ptr->pool);
    }
    
    Node* root = ctx_ptr->root;
    return { std::move(ctx_ptr), root };
}

} // namespace md
