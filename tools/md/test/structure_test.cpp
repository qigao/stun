#include "md.h"
#include <iostream>

using namespace md;

static std::string type_to_string(NodeType t) {
    switch(t) {
        case NodeType::Document: return "Document";
        case NodeType::Paragraph: return "Paragraph";
        case NodeType::Text: return "Text";
        case NodeType::List: return "List";
        case NodeType::OrderedList: return "OrderedList";
        case NodeType::ListItem: return "ListItem";
        case NodeType::Strikethrough: return "Strikethrough";
        case NodeType::Underline: return "Underline";
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
    std::cout << "=== Test 1: Lists ===\n";
    {
        std::string input = "- Item 1\n* Item 2\n1. Item 3\n2. Item 4\n";
        auto root1 = parse(input);
        dump_tree(root1.root);
        std::cout << "Test 1 done\n";
    }
    std::cout << "Test 1 scope exited\n";
    
    std::cout << "\n=== Test 2: Special Formatting ===\n";
    {
        std::string input = "~~Strikethrough~~ and <u>Underline</u>\n";
        auto root2 = parse(input);
        dump_tree(root2.root);
        std::cout << "Test 2 done\n";
    }
    std::cout << "Test 2 scope exited\n";
    
    std::cout << "All tests completed\n";
    return 0;
}
