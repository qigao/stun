#include "md_re2c.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace md_re2c;

struct SpecTest {
    int number;
    std::string markdown;
    std::string expected_html;
    std::string flags;
    int line_number;
};

std::vector<SpecTest> parse_spec_file(const std::string& filename) {
    std::vector<SpecTest> tests;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << filename << std::endl;
        return tests;
    }
    
    std::string line;
    int line_num = 0;
    int test_num = 0;
    bool in_example = false;
    bool in_markdown = false;
    bool in_html = false;
    SpecTest current_test;
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Check for example start
        if (line.find("````````````````````````````````") == 0 && line.find("example") != std::string::npos) {
            in_example = true;
            in_markdown = true;
            test_num++;
            current_test = SpecTest();
            current_test.number = test_num;
            current_test.line_number = line_num;
            continue;
        }
        
        if (!in_example) continue;
        
        // Check for section separator
        if (line == ".") {
            if (in_markdown) {
                in_markdown = false;
                in_html = true;
            } else if (in_html) {
                in_html = false;
            }
            continue;
        }
        
        // Check for example end
        if (line.find("````````````````````````````````") == 0) {
            in_example = false;
            tests.push_back(current_test);
            continue;
        }
        
        // Collect content
        if (in_markdown) {
            if (!current_test.markdown.empty()) current_test.markdown += "\n";
            current_test.markdown += line;
        } else if (in_html) {
            if (!current_test.expected_html.empty()) current_test.expected_html += "\n";
            current_test.expected_html += line;
        } else {
            // This is the flags line
            current_test.flags = line;
        }
    }
    
    return tests;
}

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
        case NodeType::TaskItem: return "TaskItem";
        case NodeType::Checkbox: return "Checkbox";
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

int main(int argc, char** argv) {
    std::string filename;
    
#ifdef SPEC_FILE
    // Use compile-time defined spec file
    filename = SPEC_FILE;
    std::cout << "Using embedded spec file: " << filename << std::endl;
#else
    // Require command-line argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <spec-file.txt>" << std::endl;
        return 1;
    }
    filename = argv[1];
#endif
    
    auto tests = parse_spec_file(filename);
    
    std::cout << "Loaded " << tests.size() << " tests from " << filename << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        std::cout << "\nTest #" << test.number << " (line " << test.line_number << ")" << std::endl;
        std::cout << "Markdown: " << test.markdown << std::endl;
        
        auto result = parse(test.markdown + "\n");
        auto root = result.root;
        
        if (root && root->children.size() > 0) {
            std::cout << "✅ PARSED" << std::endl;
            dump_tree(root);
            passed++;
        } else {
            std::cout << "❌ FAILED TO PARSE" << std::endl;
            failed++;
        }
        
        std::cout << std::string(80, '-') << std::endl;
    }
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    if (!tests.empty()) {
        std::cout << "Success rate: " << (100.0 * passed / tests.size()) << "%" << std::endl;
    }
    
    return failed > 0 ? 1 : 0;
}
