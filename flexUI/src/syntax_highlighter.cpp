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
    // A prior acquired renderer keeps its own shared lifetime when this entry
    // is replaced. The factory mutex never spans a provider callback.
    auto owner = std::shared_ptr<SyntaxHighlighter>(std::move(highlighter));
    std::lock_guard<std::mutex> lock(mutex_);
    highlighters_[lang] = std::move(owner);
}

std::shared_ptr<SyntaxHighlighter>
HighlighterFactory::acquire(const std::string& lang) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = highlighters_.find(lang);
        if (it != highlighters_.end()) return it->second;
    }
    // Provider discovery takes its own DSO registry lock. Never hold both
    // locks at once: unloading and application callbacks may run concurrently.
    auto created = std::shared_ptr<SyntaxHighlighter>(create_ts_highlighter(lang));
    if (!created) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = highlighters_.try_emplace(lang, created);
    return it->second;
}

SyntaxHighlighter* HighlighterFactory::get(const std::string& lang) {
    // Kept for older single-threaded callers. MarkdownWidget uses acquire().
    return acquire(lang).get();
}

bool HighlighterFactory::supports(const std::string& lang) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = highlighters_.find(lang);
    return it != highlighters_.end() && static_cast<bool>(it->second);
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
