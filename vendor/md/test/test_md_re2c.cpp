#include <catch2/catch_all.hpp>
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
        case NodeType::Link: return "Link";
        case NodeType::Image: return "Image";
        case NodeType::HorizontalRule: return "HorizontalRule";
        case NodeType::Table: return "Table";
        case NodeType::TableRow: return "TableRow";
        case NodeType::TableCell: return "TableCell";
        case NodeType::Strikethrough: return "Strikethrough";
        case NodeType::Underline: return "Underline";
        case NodeType::LineBreak: return "LineBreak";
        case NodeType::HtmlEntity: return "HtmlEntity";
        default: return "Unknown";
    }
}

static void dump_tree(Node* n, int indent = 0) {
    if (!n) return;
    for(int i=0; i<indent; ++i) std::cout << "  ";
    std::cout << type_to_string(n->type);
    if (!n->text.empty()) std::cout << " (\"" << n->text << "\")";
    std::cout << std::endl;
    for(auto child : n->children) dump_tree(child, indent + 1);
}

TEST_CASE("md_re2c Comprehensive Parsing", "[md_re2c]") {
    SECTION("Headers (H1, H2, H3)") {
        std::string input = "# H1\n## H2\n### H3\n";
        auto result = parse(input);
        auto* root = result.root;
        
        REQUIRE(root != nullptr);
        REQUIRE(root->children.size() == 3);
        
        CHECK(root->children[0]->type == NodeType::Header);
        CHECK(root->children[0]->level == 1);
        CHECK(root->children[1]->type == NodeType::Header);
        CHECK(root->children[1]->level == 2);
        CHECK(root->children[2]->type == NodeType::Header);
        CHECK(root->children[2]->level == 3);
    }

    SECTION("Inline Formatting (Bold, Italic)") {
        std::string input = "Normal **Bold** *Italic*\n";
        auto result = parse(input);
        auto* root = result.root;
        
        REQUIRE(root != nullptr);
        REQUIRE(root->children.size() == 1);
        auto* p = root->children[0];
        REQUIRE(p->type == NodeType::Paragraph);
        
        if (p->children.size() != 4) {
            std::cout << "Tree dump for Inline Formatting failure:" << std::endl;
            dump_tree(root);
        }
        REQUIRE(p->children.size() == 4);
        
        CHECK(p->children[0]->text == "Normal ");
        CHECK(p->children[1]->type == NodeType::Strong);
        CHECK(p->children[2]->text == " ");
        CHECK(p->children[3]->type == NodeType::Emphasis);
    }

    SECTION("Ordered and Unordered Lists") {
        std::string input = "- Item 1\n* Item 2\n1. Item 3\n2. Item 4\n";
        auto result = parse(input);
        auto* root = result.root;
        REQUIRE(root != nullptr);
        
        bool found_ul = false, found_ol = false;
        for(auto child : root->children) {
            if (child->type == NodeType::List) found_ul = true;
            if (child->type == NodeType::OrderedList) found_ol = true;
        }
        CHECK(found_ul);
        CHECK(found_ol);
    }

    SECTION("Links and Images") {
        std::string input = "[Google](https://google.com) and ![Logo](logo.png)\n";
        auto result = parse(input);
        auto* root = result.root;
        REQUIRE(root != nullptr);

        auto* p = root->children[0];
        
        bool found_link = false, found_img = false;
        for(auto child : p->children) {
            if (child->type == NodeType::Link) {
                found_link = true;
                CHECK(child->text == "https://google.com");
            }
            if (child->type == NodeType::Image) {
                found_img = true;
                CHECK(child->text == "logo.png");
            }
        }
        CHECK(found_link);
        CHECK(found_img);
    }

    SECTION("Horizontal Rules") {
        std::string input = "---\n***\n___\n<hr>\n";
        auto result = parse(input);
        auto* root = result.root;
        REQUIRE(root != nullptr);
        for(auto child : root->children) {
            if (child->type != NodeType::HorizontalRule && child->type != NodeType::Paragraph) {
                if (child->type != NodeType::HorizontalRule) {
                    dump_tree(root);
                    FAIL("Expected HorizontalRule for " + input);
                }
            }
        }
    }

    SECTION("Tables") {
        std::string input = "| Cell 1 | Cell 2 |\n|---|---|\n| Cell 3 | Cell 4 |\n";
        auto result = parse(input);
        auto* root = result.root;
        REQUIRE(root != nullptr);
        
        REQUIRE(root->children.size() == 1);
        CHECK(root->children[0]->type == NodeType::Table);
        auto* table = root->children[0];
        REQUIRE(table->children.size() == 2); 
    }

    SECTION("Special Formatting") {
        std::string input = "~~Strikethrough~~ and <u>Underline</u>\n";
        auto result = parse(input);
        auto* root = result.root;
        REQUIRE(root != nullptr);

        auto* p = root->children[0];
        
        bool found_strike = false, found_under = false;
        for(auto child : p->children) {
            if (child->type == NodeType::Strikethrough) found_strike = true;
            if (child->type == NodeType::Underline) found_under = true;
        }
        CHECK(found_strike);
        CHECK(found_under);
    }
}
