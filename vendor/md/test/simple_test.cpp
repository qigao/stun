#include "md_re2c.h"
#include <iostream>

using namespace md_re2c;

static std::string type_to_string(NodeType t) {
    switch(t) {
        case NodeType::Document: return "Document";
        case NodeType::Paragraph: return "Paragraph";
        case NodeType::Header: return "Header";
        case NodeType::List: return "List";
        case NodeType::OrderedList: return "OrderedList";
        case NodeType::ListItem: return "ListItem";
        case NodeType::Text: return "Text";
        case NodeType::Emphasis: return "Emphasis";
        case NodeType::Strong: return "Strong";
        default: return "Unknown";
    }
}

static void dump_tree(Node* n, int indent = 0) {
    for(int i=0; i<indent; ++i) std::cout << "  ";
    std::cout << type_to_string(n->type);
    if (!n->text.empty()) std::cout << " (\"" << n->text << "\")";
    std::cout << " [" << n->children.size() << " children]" << std::endl;
    for(auto& child : n->children) dump_tree(child.get(), indent + 1);
}

int main() {
    std::cout << "\n=== Test 1: Bold and Italic ===" << std::endl;
    std::string input1 = "Normal **Bold** *Italic*\n";
    std::cout << "Input: \"" << input1 << "\"" << std::endl;
    auto root1 = parse(input1);
    std::cout << "\nTree structure:" << std::endl;
    dump_tree(root1.get());
    std::cout << "Root children count: " << root1->children.size() << std::endl;
    
    std::cout << "\n=== Test 2: Lists ===" << std::endl;
    std::string input2 = "- Item 1\n* Item 2\n1. Item 3\n2. Item 4\n";
    std::cout << "Input: \"" << input2 << "\"" << std::endl;
    auto root2 = parse(input2);
    std::cout << "\nTree structure:" << std::endl;
    dump_tree(root2.get());
    
    return 0;
}
