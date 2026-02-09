#include <catch2/catch_all.hpp>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flex/bridge/renderer.h>
#include <unordered_set>

using namespace flexUI;

TEST_CASE("MarkdownWidget Parsing", "[markdown]") {
    // We need a dummy renderer for Box
    // In actual tests, we might not need a real renderer if we only check elements
    auto box = std::make_unique<Box>(nullptr); 
    auto* root = box->create("div", "root");
    box->set_root(root);

    SECTION("Basic Paragraph") {
        auto* md = box->create_widget<MarkdownWidget>("div", "md", "Hello World");
        root->append(md);
        
        // Trigger update to rebuild elements
        md->widget->update(0.0f, *md);
        
        // Check children
        // md -> [p] -> "Hello World"
        REQUIRE(md->child_count() == 1);
        auto* p = static_cast<Element*>(md->children()[0]);
        REQUIRE(p->has_class(Symbol("md-p")));
        REQUIRE(p->text() == "Hello World");
    }

    SECTION("Headers") {
        auto* md = box->create_widget<MarkdownWidget>("div", "md", "# H1\n## H2");
        root->append(md);
        
        md->widget->update(0.0f, *md);
        
        REQUIRE(md->child_count() == 2);
        auto* h1 = static_cast<Element*>(md->children()[0]);
        auto* h2 = static_cast<Element*>(md->children()[1]);
        
        REQUIRE(h1->has_class(Symbol("h1")));
        REQUIRE(h1->text() == "H1");
        
        REQUIRE(h2->has_class(Symbol("h2")));
        REQUIRE(h2->text() == "H2");
    }

    SECTION("Emphasis and Strong") {
        auto* md = box->create_widget<MarkdownWidget>("div", "md", "**Strong** and *Emphasis*");
        root->append(md);
        
        md->widget->update(0.0f, *md);
        
        REQUIRE(md->child_count() == 1);
        auto* p = static_cast<Element*>(md->children()[0]);
        REQUIRE(p->has_class(Symbol("md-p")));
        
        // Should have children for spans: [strong, text, em]
        // Actually md_parse result depends on how we implemented text_callback.
        // In our current implementation:
        // enter_span(strong) -> moves parent text (empty) to a child span (skipped if empty) -> creates span(strong) -> appends it
        // text("Strong") -> current(span strong) has no children -> span(strong)->set_text("Strong")
        // leave_span(strong)
        // text(" and ") -> current(p) has children (span strong) -> creates span(text) -> set_text(" and ") -> appends
        // enter_span(em) -> current(p) has text (none?) -> creates span(em) -> appends
        // text("Emphasis") -> current(span em) has no children -> span(em)->set_text("Emphasis")

        REQUIRE(p->child_count() == 3);
        auto* s1 = static_cast<Element*>(p->children()[0]);
        auto* s2 = static_cast<Element*>(p->children()[1]);
        auto* s3 = static_cast<Element*>(p->children()[2]);
        
        REQUIRE(s1->has_class(Symbol("md-strong")));
        REQUIRE(s1->text() == "Strong");
        
        REQUIRE(s2->text() == " and ");
        
        REQUIRE(s3->has_class(Symbol("md-em")));
        REQUIRE(s3->text() == "Emphasis");
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

TEST_CASE("C++ Tokenizer", "[syntax-highlight]") {
    
    SECTION("Keywords") {
        auto tokens = tokenize_cpp("if else for while return");
        REQUIRE(tokens.size() == 9); // 5 keywords + 4 spaces
        REQUIRE(tokens[0].type == TokenType::Keyword);
        REQUIRE(tokens[0].text == "if");
        REQUIRE(tokens[2].type == TokenType::Keyword);
        REQUIRE(tokens[2].text == "else");
    }
    
    SECTION("Types") {
        auto tokens = tokenize_cpp("int x");
        REQUIRE(tokens.size() == 3);
        REQUIRE(tokens[0].type == TokenType::Type);
        REQUIRE(tokens[0].text == "int");
        REQUIRE(tokens[2].type == TokenType::Plain);
        REQUIRE(tokens[2].text == "x");
    }
    
    SECTION("String literals") {
        auto tokens = tokenize_cpp("\"hello world\"");
        REQUIRE(tokens.size() == 1);
        REQUIRE(tokens[0].type == TokenType::String);
        REQUIRE(tokens[0].text == "\"hello world\"");
    }
    
    SECTION("Numbers") {
        auto tokens = tokenize_cpp("42 3.14 0xFF");
        REQUIRE(tokens.size() == 5);
        REQUIRE(tokens[0].type == TokenType::Number);
        REQUIRE(tokens[0].text == "42");
        REQUIRE(tokens[2].type == TokenType::Number);
        REQUIRE(tokens[2].text == "3.14");
        REQUIRE(tokens[4].type == TokenType::Number);
        REQUIRE(tokens[4].text == "0xFF");
    }
    
    SECTION("Single-line comment") {
        auto tokens = tokenize_cpp("x // comment");
        REQUIRE(tokens.size() == 3);
        REQUIRE(tokens[0].type == TokenType::Plain);
        REQUIRE(tokens[2].type == TokenType::Comment);
        REQUIRE(tokens[2].text == "// comment");
    }
    
    SECTION("Multi-line comment") {
        auto tokens = tokenize_cpp("/* multi\nline */");
        REQUIRE(tokens.size() == 1);
        REQUIRE(tokens[0].type == TokenType::Comment);
    }
    
    SECTION("Preprocessor") {
        auto tokens = tokenize_cpp("#include <iostream>");
        REQUIRE(tokens.size() == 1);
        REQUIRE(tokens[0].type == TokenType::Preprocessor);
        REQUIRE(tokens[0].text == "#include <iostream>");
    }
    
    SECTION("Complex code") {
        auto tokens = tokenize_cpp("int main() { return 0; }");
        // int main ( ) { return 0 ; }
        // Type Plain Op Op Plain Keyword Number Op Op
        bool has_type = false, has_keyword = false, has_number = false;
        for (const auto& t : tokens) {
            if (t.type == TokenType::Type && t.text == "int") has_type = true;
            if (t.type == TokenType::Keyword && t.text == "return") has_keyword = true;
            if (t.type == TokenType::Number && t.text == "0") has_number = true;
        }
        REQUIRE(has_type);
        REQUIRE(has_keyword);
        REQUIRE(has_number);
    }
}
