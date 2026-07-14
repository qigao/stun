#include "md.h"
#include <iostream>

using namespace md;

static std::string type_to_string(NodeType t) {
    switch(t) {
        case NodeType::Document: return "Document";
        case NodeType::Paragraph: return "Paragraph";
        case NodeType::Text: return "Text";
        case NodeType::Emphasis: return "Emphasis";
        case NodeType::Strong: return "Strong";
        case NodeType::Strikethrough: return "Strikethrough";
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
    std::cout << "=== Test 1: Bold ===\n";
    {
        std::string input = "Normal **Bold** text\n";
        auto root1 = parse(input);
        dump_tree(root1.root);
    }
    
    std::cout << "\n=== Test 2: Italic ===\n";
    {
        std::string input = "Normal *Italic* text\n";
        auto root2 = parse(input);
        dump_tree(root2.root);
    }
    
    std::cout << "\n=== Test 3: Bold and Italic ===\n";
    {
        std::string input = "Normal **Bold** *Italic* text\n";
        auto root3= parse(input);
        dump_tree(root3.root);
    }
    
    std::cout << "\n=== Test 4: Strikethrough ===\n";
    {
        std::string input = "Normal ~~Strike~~ text\n";
        auto root4 = parse(input);
        dump_tree(root4.root);
    }
    
    std::cout << "\n=== Test 5: Nested ===\n";
    {
        std::string input = "Normal **Bold *and italic* text** here\n";
        auto root5 = parse(input);
        dump_tree(root5.root);
    }
    
    return 0;
}
