#include <tinytest.h>
#undef group
#include "test_support.h"
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flex/bridge/renderer.h>
#include <unordered_set>

using namespace flexUI;

spec("MarkdownWidget Parsing") {
    // We need a dummy renderer for Box
    // In actual tests, we might not need a real renderer if we only check elements
    auto box = std::make_unique<Box>(nullptr); 
    auto* root = box->create("div");
    box->set_root(root);

    it("Basic Paragraph") {
        auto* md = box->create_widget<MarkdownWidget>("div", "md", "Hello World");
        root->append(md);
        
        // Trigger update to rebuild elements
        md->widget->update(0.0f, *md);
        
        // Check children
        // md -> [p] -> "Hello World"
        check(md->child_count() == 1);
        auto* p = static_cast<Element*>(md->children()[0]);
        check(p->has_class(Symbol("md-p")));
        check(p->text() == "Hello World");
    }

    it("Headers") {
        auto* md = box->create_widget<MarkdownWidget>("div", "md", "# H1\n## H2");
        root->append(md);
        
        md->widget->update(0.0f, *md);
        
        check(md->child_count() == 2);
        auto* h1 = static_cast<Element*>(md->children()[0]);
        auto* h2 = static_cast<Element*>(md->children()[1]);
        
        check(h1->has_class(Symbol("h1")));
        check(h1->text() == "H1");
        
        check(h2->has_class(Symbol("h2")));
        check(h2->text() == "H2");
    }

    it("Emphasis and Strong") {
        auto* md = box->create_widget<MarkdownWidget>("div", "md", "**Strong** and *Emphasis*");
        root->append(md);
        
        md->widget->update(0.0f, *md);
        
        check(md->child_count() == 1);
        auto* p = static_cast<Element*>(md->children()[0]);
        check(p->has_class(Symbol("md-p")));
        
        // Should have children for spans: [strong, text, em]
        // Actually md_parse result depends on how we implemented text_callback.
        // In our current implementation:
        // enter_span(strong) -> moves parent text (empty) to a child span (skipped if empty) -> creates span(strong) -> appends it
        // text("Strong") -> current(span strong) has no children -> span(strong)->set_text("Strong")
        // leave_span(strong)
        // text(" and ") -> current(p) has children (span strong) -> creates span(text) -> set_text(" and ") -> appends
        // enter_span(em) -> current(p) has text (none?) -> creates span(em) -> appends
        // text("Emphasis") -> current(span em) has no children -> span(em)->set_text("Emphasis")

        check(p->child_count() == 3);
        auto* s1 = static_cast<Element*>(p->children()[0]);
        auto* s2 = static_cast<Element*>(p->children()[1]);
        auto* s3 = static_cast<Element*>(p->children()[2]);
        
        check(s1->has_class(Symbol("md-strong")));
        check(s1->text() == "Strong");
        
        check(s2->text() == " and ");
        
        check(s3->has_class(Symbol("md-em")));
        check(s3->text() == "Emphasis");
    }
}


// ============================================================================
// C++ Syntax Highlighting Tests
// ============================================================================

// Copy of tokenizer for testing (same as in markdown_widget.cpp)
namespace {

enum class TokenType { Keyword, Type, String, Number, Comment, Preprocessor, Operator, Plain };

struct Token {
    TokenType type;
    std::string text;
};

static const std::unordered_set<std::string> cpp_keywords = {
    "auto", "break", "case", "catch", "class", "const", "constexpr", "continue",
    "default", "delete", "do", "else", "enum", "explicit", "export", "extern",
    "false", "for", "friend", "goto", "if", "inline", "mutable", "namespace",
    "new", "noexcept", "nullptr", "operator", "private", "protected", "public",
    "register", "return", "sizeof", "static", "static_cast", "dynamic_cast",
    "reinterpret_cast", "const_cast", "struct", "switch", "template", "this",
    "throw", "true", "try", "typedef", "typeid", "typename", "union", "using",
    "virtual", "void", "volatile", "while", "override", "final", "concept", "requires"
};

static const std::unordered_set<std::string> cpp_types = {
    "int", "char", "float", "double", "bool", "long", "short", "unsigned", "signed",
    "size_t", "string", "vector", "map", "set", "unique_ptr", "shared_ptr", "weak_ptr",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t"
};

static std::vector<Token> tokenize_cpp(std::string_view code) {
    std::vector<Token> tokens;
    size_t i = 0;
    
    while (i < code.size()) {
        // Skip whitespace but preserve it
        if (std::isspace(static_cast<unsigned char>(code[i]))) {
            size_t start = i;
            while (i < code.size() && std::isspace(static_cast<unsigned char>(code[i])) && code[i] != '\n') i++;
            tokens.push_back({TokenType::Plain, std::string(code.substr(start, i - start))});
            continue;
        }
        
        // Single-line comment
        if (i + 1 < code.size() && code[i] == '/' && code[i+1] == '/') {
            size_t start = i;
            while (i < code.size() && code[i] != '\n') i++;
            tokens.push_back({TokenType::Comment, std::string(code.substr(start, i - start))});
            continue;
        }
        
        // Multi-line comment
        if (i + 1 < code.size() && code[i] == '/' && code[i+1] == '*') {
            size_t start = i;
            i += 2;
            while (i + 1 < code.size() && !(code[i] == '*' && code[i+1] == '/')) i++;
            if (i + 1 < code.size()) i += 2;
            tokens.push_back({TokenType::Comment, std::string(code.substr(start, i - start))});
            continue;
        }
        
        // Preprocessor
        if (code[i] == '#') {
            size_t start = i;
            while (i < code.size() && code[i] != '\n') i++;
            tokens.push_back({TokenType::Preprocessor, std::string(code.substr(start, i - start))});
            continue;
        }
        
        // String literal
        if (code[i] == '"' || code[i] == '\'') {
            char quote = code[i];
            size_t start = i++;
            while (i < code.size() && code[i] != quote) {
                if (code[i] == '\\' && i + 1 < code.size()) i++;
                i++;
            }
            if (i < code.size()) i++;
            tokens.push_back({TokenType::String, std::string(code.substr(start, i - start))});
            continue;
        }
        
        // Number
        if (std::isdigit(static_cast<unsigned char>(code[i])) || 
            (code[i] == '.' && i + 1 < code.size() && std::isdigit(static_cast<unsigned char>(code[i+1])))) {
            size_t start = i;
            while (i < code.size() && (std::isalnum(static_cast<unsigned char>(code[i])) || 
                   code[i] == '.' || code[i] == 'x' || code[i] == 'X')) i++;
            tokens.push_back({TokenType::Number, std::string(code.substr(start, i - start))});
            continue;
        }
        
        // Identifier or keyword
        if (std::isalpha(static_cast<unsigned char>(code[i])) || code[i] == '_') {
            size_t start = i;
            while (i < code.size() && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_')) i++;
            std::string word(code.substr(start, i - start));
            if (cpp_keywords.count(word)) {
                tokens.push_back({TokenType::Keyword, word});
            } else if (cpp_types.count(word)) {
                tokens.push_back({TokenType::Type, word});
            } else {
                tokens.push_back({TokenType::Plain, word});
            }
            continue;
        }
        
        // Operators and punctuation
        tokens.push_back({TokenType::Operator, std::string(1, code[i])});
        i++;
    }
    return tokens;
}

} // anonymous namespace

spec("C++ Tokenizer") {
    
    it("Keywords") {
        auto tokens = tokenize_cpp("if else for while return");
        check(tokens.size() == 9); // 5 keywords + 4 spaces
        check(tokens[0].type == TokenType::Keyword);
        check(tokens[0].text == "if");
        check(tokens[2].type == TokenType::Keyword);
        check(tokens[2].text == "else");
    }
    
    it("Types") {
        auto tokens = tokenize_cpp("int x");
        check(tokens.size() == 3);
        check(tokens[0].type == TokenType::Type);
        check(tokens[0].text == "int");
        check(tokens[2].type == TokenType::Plain);
        check(tokens[2].text == "x");
    }
    
    it("String literals") {
        auto tokens = tokenize_cpp("\"hello world\"");
        check(tokens.size() == 1);
        check(tokens[0].type == TokenType::String);
        check(tokens[0].text == "\"hello world\"");
    }
    
    it("Numbers") {
        auto tokens = tokenize_cpp("42 3.14 0xFF");
        check(tokens.size() == 5);
        check(tokens[0].type == TokenType::Number);
        check(tokens[0].text == "42");
        check(tokens[2].type == TokenType::Number);
        check(tokens[2].text == "3.14");
        check(tokens[4].type == TokenType::Number);
        check(tokens[4].text == "0xFF");
    }
    
    it("Single-line comment") {
        auto tokens = tokenize_cpp("x // comment");
        check(tokens.size() == 3);
        check(tokens[0].type == TokenType::Plain);
        check(tokens[2].type == TokenType::Comment);
        check(tokens[2].text == "// comment");
    }
    
    it("Multi-line comment") {
        auto tokens = tokenize_cpp("/* multi\nline */");
        check(tokens.size() == 1);
        check(tokens[0].type == TokenType::Comment);
    }
    
    it("Preprocessor") {
        auto tokens = tokenize_cpp("#include <iostream>");
        check(tokens.size() == 1);
        check(tokens[0].type == TokenType::Preprocessor);
        check(tokens[0].text == "#include <iostream>");
    }
    
    it("Complex code") {
        auto tokens = tokenize_cpp("int main() { return 0; }");
        // int main ( ) { return 0 ; }
        // Type Plain Op Op Plain Keyword Number Op Op
        bool has_type = false, has_keyword = false, has_number = false;
        for (const auto& t : tokens) {
            if (t.type == TokenType::Type && t.text == "int") has_type = true;
            if (t.type == TokenType::Keyword && t.text == "return") has_keyword = true;
            if (t.type == TokenType::Number && t.text == "0") has_number = true;
        }
        check(has_type);
        check(has_keyword);
        check(has_number);
    }
}
