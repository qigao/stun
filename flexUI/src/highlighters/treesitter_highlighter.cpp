/*
 * flexUI - Tree-sitter based Syntax Highlighter
 * 
 * This file only calls plugins. All parsing logic is in plugins.
 */

#include <flexUI/syntax_highlighter.h>
#include <flexUI/ts_plugin.h>
#include <algorithm>

namespace flexUI {

const TSPluginInfo* get_ts_plugin(const std::string& name);
void load_ts_plugins_from_dir(const std::string& dir);

static HighlightType convert_type(TSHighlightType t) {
    switch (t) {
        case TS_HIGHLIGHT_KEYWORD:      return HighlightType::Keyword;
        case TS_HIGHLIGHT_TYPE:         return HighlightType::Type;
        case TS_HIGHLIGHT_FUNCTION:     return HighlightType::Function;
        case TS_HIGHLIGHT_VARIABLE:     return HighlightType::Variable;
        case TS_HIGHLIGHT_STRING:       return HighlightType::String;
        case TS_HIGHLIGHT_NUMBER:       return HighlightType::Number;
        case TS_HIGHLIGHT_COMMENT:      return HighlightType::Comment;
        case TS_HIGHLIGHT_PREPROCESSOR: return HighlightType::Preprocessor;
        case TS_HIGHLIGHT_OPERATOR:     return HighlightType::Operator;
        case TS_HIGHLIGHT_PUNCTUATION:  return HighlightType::Punctuation;
        case TS_HIGHLIGHT_CONSTANT:     return HighlightType::Constant;
        default:                        return HighlightType::Plain;
    }
}

class TreeSitterHighlighter : public SyntaxHighlighter {
public:
    explicit TreeSitterHighlighter(const TSPluginInfo* plugin, std::string name)
        : plugin_(plugin), lang_name_(std::move(name)) {}

    std::vector<HighlightToken> highlight(std::string_view source) override {
        std::vector<HighlightToken> tokens;
        if (source.empty() || !plugin_->highlight) return tokens;

        TSHighlightResult* result = plugin_->highlight(source.data(), static_cast<uint32_t>(source.size()));
        if (!result) return tokens;

        for (uint32_t i = 0; i < result->count; i++) {
            tokens.push_back({
                convert_type(result->tokens[i].type),
                result->tokens[i].start,
                result->tokens[i].length
            });
        }

        if (plugin_->free_result) plugin_->free_result(result);

        std::sort(tokens.begin(), tokens.end(), [](const auto& a, const auto& b) {
            return a.start < b.start;
        });

        return fill_gaps(tokens, source.size());
    }

    const char* language() const override { return lang_name_.c_str(); }

private:
    const TSPluginInfo* plugin_;
    std::string lang_name_;

    std::vector<HighlightToken> fill_gaps(const std::vector<HighlightToken>& tokens, size_t len) {
        std::vector<HighlightToken> result;
        size_t pos = 0;

        for (const auto& tok : tokens) {
            if (tok.start > pos) {
                result.push_back({HighlightType::Plain, pos, tok.start - pos});
            }
            if (tok.start >= pos) {
                result.push_back(tok);
                pos = tok.start + tok.length;
            }
        }

        if (pos < len) {
            result.push_back({HighlightType::Plain, pos, len - pos});
        }
        return result;
    }
};

void init_treesitter_highlighters() {
    load_ts_plugins_from_dir(".");
}

SyntaxHighlighter* create_ts_highlighter(const std::string& lang) {
    const TSPluginInfo* plugin = get_ts_plugin(lang);
    if (!plugin) return nullptr;
    return new TreeSitterHighlighter(plugin, lang);
}

} // namespace flexUI
