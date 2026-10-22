#pragma once

#include "md.h"
#include "md_block_parser.h"
#include <string_view>
#include <vector>

namespace md {

struct DelimiterInfo;  // forward declaration

class InlineParser {
public:
    explicit InlineParser(ParseContext* context) : context_(context) {}
    
    Node* transform(const Block& block);
    
private:
    void parse_inlines(std::string_view text, Node* parent);
    void process_emphasis(std::vector<Node*>& nodes, std::vector<DelimiterInfo>& delims);
    
    Node* create_node(NodeType type);
    Node* create_text(std::string_view text);
    
    ParseContext* context_;
};

Node* blocks_to_ast(const Block& root, ParseContext* context);

} // namespace md
