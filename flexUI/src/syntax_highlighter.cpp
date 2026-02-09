/*
 * flexUI - Syntax Highlighter Implementation
 */

#include <flexUI/syntax_highlighter.h>

namespace flexUI {

void init_treesitter_highlighters();
SyntaxHighlighter* create_ts_highlighter(const std::string& lang);

HighlighterFactory& HighlighterFactory::instance() {
    static HighlighterFactory factory;
    return factory;
}

namespace {
    struct Initializer {
        Initializer() { init_treesitter_highlighters(); }
    } init;
}

void HighlighterFactory::register_highlighter(const std::string& lang, 
                                               std::unique_ptr<SyntaxHighlighter> highlighter) {
    highlighters_[lang] = std::move(highlighter);
}

SyntaxHighlighter* HighlighterFactory::get(const std::string& lang) {
    auto it = highlighters_.find(lang);
    if (it != highlighters_.end()) {
        return it->second.get();
    }
    
    // Try to create from loaded plugin
    SyntaxHighlighter* hl = create_ts_highlighter(lang);
    if (hl) {
        highlighters_[lang] = std::unique_ptr<SyntaxHighlighter>(hl);
        return hl;
    }
    return nullptr;
}

bool HighlighterFactory::supports(const std::string& lang) const {
    return highlighters_.find(lang) != highlighters_.end();
}

const char* highlight_class(HighlightType type) {
    switch (type) {
        case HighlightType::Keyword:      return "hl-keyword";
        case HighlightType::Type:         return "hl-type";
        case HighlightType::Function:     return "hl-function";
        case HighlightType::Variable:     return "hl-variable";
        case HighlightType::String:       return "hl-string";
        case HighlightType::Number:       return "hl-number";
        case HighlightType::Comment:      return "hl-comment";
        case HighlightType::Preprocessor: return "hl-preproc";
        case HighlightType::Operator:     return "hl-operator";
        case HighlightType::Punctuation:  return "hl-punct";
        case HighlightType::Constant:     return "hl-constant";
        case HighlightType::Attribute:    return "hl-attr";
        case HighlightType::Tag:          return "hl-tag";
        case HighlightType::Property:     return "hl-property";
        case HighlightType::Namespace:    return "hl-namespace";
        default:                          return "hl-plain";
    }
}

} // namespace flexUI
