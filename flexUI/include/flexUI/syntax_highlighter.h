/*
 * flexUI - Syntax Highlighter Interface
 * 
 * Abstract interface for syntax highlighting.
 * Implementations can use simple lexers or Tree-sitter.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>

namespace flexUI {

// Token types for syntax highlighting (One Dark Pro compatible)
enum class HighlightType {
    Plain,          // Default text
    Keyword,        // if, for, while, class, etc.
    Type,           // int, string, bool, etc.
    Function,       // Function names
    Variable,       // Variable names
    String,         // "string literals"
    Number,         // 42, 3.14, 0xFF
    Comment,        // // and /* */
    Preprocessor,   // #include, #define
    Operator,       // +, -, *, /, etc.
    Punctuation,    // (), {}, [], etc.
    Constant,       // true, false, nullptr
    Attribute,      // @decorator, [[attribute]]
    Tag,            // HTML/XML tags
    Property,       // Object properties
    Namespace,      // Namespace names
};

struct HighlightToken {
    HighlightType type;
    size_t start;   // Byte offset in source
    size_t length;  // Byte length
};

// Abstract highlighter interface
class SyntaxHighlighter {
public:
    virtual ~SyntaxHighlighter() = default;
    
    // Tokenize source code and return highlight tokens
    virtual std::vector<HighlightToken> highlight(std::string_view source) = 0;
    
    // Get language name
    virtual const char* language() const = 0;
};

// Factory to create highlighters by language name
class HighlighterFactory {
public:
    static HighlighterFactory& instance();
    
    // Register a highlighter for a language
    void register_highlighter(const std::string& lang, std::unique_ptr<SyntaxHighlighter> highlighter);
    
    // Get highlighter for a language (returns nullptr if not found)
    SyntaxHighlighter* get(const std::string& lang);
    
    // Check if a language is supported
    bool supports(const std::string& lang) const;

private:
    std::unordered_map<std::string, std::unique_ptr<SyntaxHighlighter>> highlighters_;
};

// Map HighlightType to CSS class name
const char* highlight_class(HighlightType type);

} // namespace flexUI
