#include <tinytest.hpp>
#undef group
#include "test_support.h"
#include <flexUI/syntax_highlighter.h>
#include <string>
#include <string_view>

namespace flexUI {
bool load_ts_plugin(const std::string& path);
void unload_all_ts_plugins();
}

using namespace flexUI;

spec("C11 code highlighter DSO lease lifetime") {
    it("retains the C provider during a registry unload and reload") {
        const std::string path = TS_LANG_C_PATH;
        check(!path.empty());
        check(load_ts_plugin(path));
        auto* highlighter = HighlighterFactory::instance().get("c");
        check(highlighter != nullptr);
        if (!highlighter) return;
        const std::string source =
            "int main(void) { return 7; }";
        const auto first = highlighter->highlight(source);
        check(!first.empty());
        bool saw_keyword = false;
        for (const auto& token : first) {
            check(token.start <= source.size());
            check(token.length <= source.size() - token.start);
            if (token.type == HighlightType::Keyword &&
                std::string_view(source).substr(
                    token.start, token.length) == "return")
                saw_keyword = true;
        }
        check(saw_keyword);

        // A registry unload must not dlclose a plugin still owned by a
        // cached TreeSitterHighlighter. No borrowed TSPluginInfo may dangle.
        unload_all_ts_plugins();
        const auto after = highlighter->highlight(source);
        check(!after.empty());
        check(after.size() == first.size());
        check(load_ts_plugin(path));
        const auto again = HighlighterFactory::instance().get("c");
        check(again == highlighter);
        check(!again->highlight(source).empty());
        unload_all_ts_plugins();
        check(!highlighter->highlight(source).empty());
    }
}
