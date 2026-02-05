#pragma once

#include "md_re2c.h"
#include "md_block_parser.h"
#include <string_view>
#include <vector>

namespace md_re2c {

struct DelimiterInfo;  // forward declaration

class InlineParser {
public:
    explicit InlineParser(MemoryPool* pool) : pool_(pool) {}
    
    Node* transform(const Block& block);
    
private:
    void parse_inlines(std::string_view text, Node* parent);
    void process_emphasis(std::vector<Node*>& nodes, std::vector<DelimiterInfo>& delims);
    
    Node* create_node(NodeType type);
    Node* create_text(std::string_view text);
    
    MemoryPool* pool_;
};

Node* blocks_to_ast(const Block& root, MemoryPool* pool);

} // namespace md_re2c
