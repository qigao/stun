#include "md.h"
#include <iostream>

using namespace md;

static std::string type_to_string(NodeType t) {
    switch(t) {
        case NodeType::Document: return "Document";
        case NodeType::Paragraph: return "Paragraph";
        case NodeType::List: return "List";
        case NodeType::OrderedList: return "OrderedList";
        case NodeType::ListItem: return "ListItem";
        case NodeType::Text: return "Text";
        default: return "Unknown";
    }
}

static void dump_tree(Node* n, int indent = 0) {
    for(int i=0; i<indent; ++i) std::cout << "  ";
    std::cout << type_to_string(n->type);
    if (!n->text.empty()) std::cout << " (\"" << n->text << "\")";
    std::cout << std::endl;
    for(auto& child : n->children) dump_tree(child, indent + 1);
}

int main() {
    std::cout << "=== Test 1: Indented List ===" << std::endl;
    std::string input1 = " * Item 1\n * Item 2\n";
    std::cout << "Input: \"" << input1 << "\"" << std::endl;
    auto root1 = parse(input1);
    std::cout << "\nTree structure:" << std::endl;
    dump_tree(root1.root);
    
    std::cout << "\n=== Test 2: Tasklist-like ===" << std::endl;
    std::string input2 = " * [x] foo\n * [ ] bar\n";
    std::cout << "Input: \"" << input2 << "\"" << std::endl;
    auto root2 = parse(input2);
    std::cout << "\nTree structure:" << std::endl;
    dump_tree(root2.root);
    
    return 0;
}
