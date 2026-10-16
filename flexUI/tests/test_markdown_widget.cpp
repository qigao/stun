#include <tinytest.hpp>
#undef group
#include "test_support.h"
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/syntax_highlighter.h>
#include <flex/bridge/renderer.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

using namespace flexUI;

// Test-only render highlighters: the authoritative grammar has already been
// parsed by Stun C11. These produce only byte-range presentation hints.
namespace {
class C11FixtureHighlighter final : public SyntaxHighlighter {
public:
    std::vector<HighlightToken> highlight(std::string_view text) override {
        const auto pos = text.find("return");
        if (pos == std::string_view::npos) return {};
        return {{HighlightType::Keyword, pos, 6u}};
    }
    const char* language() const override { return "stun-c11-fixture"; }
};
class C11InvalidHighlighter final : public SyntaxHighlighter {
public:
    std::vector<HighlightToken> highlight(std::string_view text) override {
        return {{HighlightType::Keyword, text.size() + 1u, 3u}};
    }
    const char* language() const override { return "stun-c11-invalid"; }
};
class C11Utf8SplitHighlighter final : public SyntaxHighlighter {
public:
    std::vector<HighlightToken> highlight(std::string_view text) override {
        const size_t at = text.find("é");
        if (at == std::string_view::npos) return {};
        // Start is valid but the end falls in the UTF-8 continuation byte.
        return {{HighlightType::Keyword, at, 1u}};
    }
    const char* language() const override { return "stun-c11-split"; }
};
class C11FloodHighlighter final : public SyntaxHighlighter {
public:
    std::vector<HighlightToken> highlight(std::string_view text) override {
        const size_t count = std::min(text.size(), size_t{1200});
        std::vector<HighlightToken> result;
        result.reserve(count);
        for (size_t i = 0u; i < count; ++i)
            result.push_back({HighlightType::Keyword, i, 1u});
        return result;
    }
    const char* language() const override { return "stun-c11-flood"; }
};
class C11CountingHighlighter final : public SyntaxHighlighter {
public:
    explicit C11CountingHighlighter(std::shared_ptr<size_t> calls)
        : calls_(std::move(calls)) {}
    std::vector<HighlightToken> highlight(std::string_view) override {
        ++*calls_;
        return {};
    }
    const char* language() const override { return "stun-c11-counted"; }
private:
    std::shared_ptr<size_t> calls_;
};
std::string code_line_text(const Element* line) {
    std::string answer = line->text();
    for (auto* node : line->children())
        answer += static_cast<const Element*>(node)->text();
    return answer;
}
} // namespace

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

    // #186: retain the existing production C++ md::parse consumer while
    // evaluating the C11 replacement in a separate RichDoc/Tango harness.
    // This verifies the *real* MarkdownWidget element hierarchy, not HTML.
    it("GFM task items preserve checkbox state in the existing widget") {
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md", "- [x] shipped\n- [ ] pending\n");
        root->append(md);
        md->widget->update(0.0f, *md);

        check(md->child_count() == 1);
        if (md->child_count() == 1) {
            auto* list = static_cast<Element*>(md->children()[0]);
            check(list->has_class(Symbol("md-ul")));
            check(list->child_count() == 2);
            if (list->child_count() == 2) {
                auto* checked = static_cast<Element*>(list->children()[0]);
                auto* unchecked = static_cast<Element*>(list->children()[1]);
                check(checked->has_class(Symbol("md-task-item")));
                check(unchecked->has_class(Symbol("md-task-item")));
                check(checked->child_count() >= 1);
                check(unchecked->child_count() >= 1);
                if (checked->child_count() >= 1 &&
                    unchecked->child_count() >= 1) {
                    auto* checked_box =
                        static_cast<Element*>(checked->children()[0]);
                    auto* unchecked_box =
                        static_cast<Element*>(unchecked->children()[0]);
                    check(checked_box->has_class(Symbol("md-checkbox")));
                    check(unchecked_box->has_class(Symbol("md-checkbox")));
                    check(checked_box->text() == "[x] ");
                    check(unchecked_box->text() == "[ ] ");
                }
            }
        }
    }

    it("GFM table projection and source replacement retain one widget owner") {
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md", "| H | V |\n| --- | --- |\n| a | b |\n");
        root->append(md);
        md->widget->update(0.0f, *md);

        check(md->child_count() == 1);
        if (md->child_count() == 1) {
            auto* table = static_cast<Element*>(md->children()[0]);
            check(table->has_class(Symbol("md-table")));
            check(table->child_count() == 2);
            if (table->child_count() == 2) {
                for (const auto* row : table->children()) {
                    const auto* element = static_cast<const Element*>(row);
                    check(element->has_class(Symbol("md-tr")));
                    check(element->child_count() == 2);
                }
            }
        }

        // C11 projection replaces the visible children once per changed source,
        // with no legacy parser fallback.
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        widget->set_markdown("- [ ] pending\n");
        md->widget->update(0.0f, *md);
        check(md->child_count() == 1);
        if (md->child_count() == 1) {
            auto* list = static_cast<Element*>(md->children()[0]);
            check(list->has_class(Symbol("md-ul")));
            check(list->child_count() == 1);
        }
        widget->set_markdown("| H | V |\n| --- | --- |\n| c | d |\n");
        md->widget->update(0.0f, *md);
        check(md->child_count() == 1);
        if (md->child_count() == 1) {
            auto* table = static_cast<Element*>(md->children()[0]);
            check(table->has_class(Symbol("md-table")));
        }
    }

    // #186/#228: preserve explicit thematic-break rendering as well as the
    // new blank-only C11 Task List → root Table regression below.
    // GFM tables cannot legally contain block/task nodes in their cells.
    it("composes Unicode heading tasks and table across explicit list close") {
        static const char* const sources[] = {
            "# 日本語 café 😀 Alpha\n\n"
            "- [x] 発送済み\n- [ ] 待機中\n\n---\n\n"
            "| 城市 | Price |\n| :--- | ---: |\n| 東京 | €20 |\n",
            "# Ελληνικά café 😀 Beta\n\n"
            "- [ ] review\n- [x] 完了\n\n---\n\n"
            "| City | Price |\n| :--- | ---: |\n| Αθήνα | £30 |\n"
        };
        static const char* const headings[] = {
            "日本語 café 😀 Alpha", "Ελληνικά café 😀 Beta"
        };
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-unicode-gfm-186", sources[0]);
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (widget == nullptr) return;
        for (size_t round = 0u; round < 8u; ++round) {
            const size_t index = round % 2u;
            widget->set_markdown(sources[index]);
            widget->update(0.0f, *md);
            check(widget->markdown() == sources[index]);
            check(md->parent_elem() == root);
            check(md->child_count() == 4u);
            if (md->child_count() != 4u) return;

            auto* heading = static_cast<Element*>(md->children()[0]);
            auto* list = static_cast<Element*>(md->children()[1]);
            auto* hr = static_cast<Element*>(md->children()[2]);
            auto* table = static_cast<Element*>(md->children()[3]);
            check(heading->has_class(Symbol("md-header")));
            check(heading->has_class(Symbol("h1")));
            check(heading->text() == headings[index]);
            check(list->has_class(Symbol("md-ul")));
            check(list->child_count() == 2u);
            check(hr->has_class(Symbol("md-hr")));
            check(table->has_class(Symbol("md-table")));
            check(table->child_count() == 2u);

            if (list->child_count() == 2u) {
                auto* first = static_cast<Element*>(list->children()[0]);
                auto* second = static_cast<Element*>(list->children()[1]);
                check(first->has_class(Symbol("md-task-item")));
                check(second->has_class(Symbol("md-task-item")));
                check(first->child_count() >= 1u);
                check(second->child_count() >= 1u);
                if (first->child_count() != 0u && second->child_count() != 0u) {
                    auto* first_checkbox =
                        static_cast<Element*>(first->children()[0]);
                    auto* second_checkbox =
                        static_cast<Element*>(second->children()[0]);
                    check(first_checkbox->has_class(Symbol("md-checkbox")));
                    check(second_checkbox->has_class(Symbol("md-checkbox")));
                    check(first_checkbox->text() == (index == 0u ? "[x] " : "[ ] "));
                    check(second_checkbox->text() == (index == 0u ? "[ ] " : "[x] "));
                }
            }
            if (table->child_count() == 2u) {
                for (const auto* child : table->children()) {
                    const auto* row = static_cast<const Element*>(child);
                    check(row->has_class(Symbol("md-tr")));
                    check(row->child_count() == 2u);
                }
            }
            const Element* const original_heading = heading;
            const Element* const original_list = list;
            const Element* const original_hr = hr;
            const Element* const original_table = table;
            widget->set_markdown(sources[index]);
            widget->update(0.0f, *md);
            check(md->child_count() == 4u);
            if (md->child_count() == 4u) {
                check(md->children()[0] == original_heading);
                check(md->children()[1] == original_list);
                check(md->children()[2] == original_hr);
                check(md->children()[3] == original_table);
            }
            check(md->parent_elem() == root);
        }
    }

    // #229: the real shipping widget must now consume the canonical C11
    // RichDoc, not reproduce the incorrect legacy C++ List->Table nesting.
    it("uses canonical C11 task list then root table across blank separator") {
        const std::string markdown =
            "# 日本語 café 😀 Alpha\n\n"
            "- [x] 発送済み\n- [ ] 待機中\n\n"
            "| City | Price |\n| :--- | ---: |\n| 東京 | €20 |\n";
        auto* md=box->create_widget<MarkdownWidget>(
            "div","md-c11-root-list-table-229",markdown);
        root->append(md);
        auto* widget=static_cast<MarkdownWidget*>(md->widget);
        check(widget!=nullptr);
        if(!widget)return;
        widget->update(0.0f,*md);
        check(md->child_count()==3u);
        if(md->child_count()!=3u)return;
        auto* header=static_cast<Element*>(md->children()[0]);
        auto* list=static_cast<Element*>(md->children()[1]);
        auto* table=static_cast<Element*>(md->children()[2]);
        check(header->has_class(Symbol("md-header")));
        check(header->text()=="日本語 café 😀 Alpha");
        check(list->has_class(Symbol("md-ul")));
        check(list->child_count()==2u);
        check(table->has_class(Symbol("md-table")));
        check(table->child_count()==2u);
        if (table->child_count() == 2u) {
            for (auto* node : table->children()) {
                const auto* row = static_cast<const Element*>(node);
                check(row->child_count() == 2u);
                if (row->child_count() != 2u) continue;
                auto* left = static_cast<Element*>(row->child_at(0u));
                auto* right = static_cast<Element*>(row->child_at(1u));
                check(left->has_class(Symbol("md-align-left")));
                check(right->has_class(Symbol("md-align-right")));
                const auto* left_meta = left->attribute("data-gfm-align");
                const auto* right_meta = right->attribute("data-gfm-align");
                check(left_meta != nullptr && *left_meta == "left");
                check(right_meta != nullptr && *right_meta == "right");
            }
        }
        if(list->child_count()==2u) {
            auto* done=static_cast<Element*>(list->children()[0]);
            auto* pending=static_cast<Element*>(list->children()[1]);
            check(done->has_class(Symbol("md-task-item")));
            check(pending->has_class(Symbol("md-task-item")));
            check(done->child_count()>=1u);
            check(pending->child_count()>=1u);
            if(done->child_count()&&pending->child_count()) {
                auto* a=static_cast<Element*>(done->children()[0]);
                auto* b=static_cast<Element*>(pending->children()[0]);
                check(a->has_class(Symbol("md-checkbox")));
                check(b->has_class(Symbol("md-checkbox")));
                check(a->text()=="[x] ");
                check(b->text()=="[ ] ");
            }
        }
        const Element* const original=header;
        widget->set_markdown(markdown);
        widget->update(0.0f,*md);
        check(md->child_count()==3u);
        if(md->child_count()==3u) check(md->children()[0]==original);

        // A 1 MiB over-budget update MUST NOT silently use md::parse or
        // publish an incomplete tree. Previously valid children remain.
        std::string over_budget(1024u*1024u,'x');
        widget->set_markdown(over_budget);
        widget->update(0.0f,*md);
        check(md->child_count()==3u);
        if(md->child_count()==3u)check(md->children()[0]==original);
        widget->set_markdown(markdown);
        widget->update(0.0f,*md);
        check(md->child_count()==3u);
        check(widget->markdown()==markdown);
    }

    it("replaces a validated Element child set without a partial tree") {
        auto* host = box->create("div", "md-atomic-child-host-231");
        root->append(host);
        auto* a = box->create("span");
        auto* b = box->create("span");
        auto* c = box->create("span");
        host->replace_children({a, b});
        check(host->child_count() == 2u);
        check(a->parent_elem() == host);
        check(b->parent_elem() == host);

        bool rejected = false;
        try {
            host->replace_children({c, c});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        check(host->child_count() == 2u);
        check(host->child_at(0) == a);
        check(host->child_at(1) == b);
        check(c->parent_elem() == nullptr);

        rejected = false;
        try {
            host->replace_children({host});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        check(host->child_at(0) == a);
        check(host->child_at(1) == b);

        host->replace_children({b, c});
        check(host->child_count() == 2u);
        check(host->child_at(0) == b);
        check(host->child_at(1) == c);
        check(a->parent_elem() == nullptr);
        check(b->parent_elem() == host);
        check(c->parent_elem() == host);
    }

    // #231: even if the parser accepts the input, a later projection
    // capacity rejection must not touch the visible Element tree.
    it("rolls back bounded detached projection and recovers without fallback") {
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-c11-stage-rollback-231", "# 初期 😀\n", 3u);
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        auto* original = static_cast<Element*>(md->children()[0]);
        check(original->text() == "初期 😀");
        const std::size_t owner_count = box->owned_element_count();

        // The heading/list/task item fit the budget, but the checkbox exceeds
        // it with one child already attached to a detached parent.
        widget->set_markdown(
            "# 次へ 😀\n\n- [x] 発送済み\n\n"
            "| City | Price |\n| --- | ---: |\n| 東京 | €20 |\n");
        for (size_t attempt = 0; attempt < 8u; ++attempt) {
            widget->update(0.0f, *md);
            check(md->child_count() == 1u);
            if (md->child_count() != 1u) return;
            check(md->children()[0] == original);
            check(static_cast<Element*>(md->children()[0])->text() == "初期 😀");
            check(box->owned_element_count() == owner_count);
        }

        widget->set_markdown("# 回復 ✅\n");
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u) {
            auto* recovered = static_cast<Element*>(md->children()[0]);
            check(recovered->has_class(Symbol("md-header")));
            check(recovered->text() == "回復 ✅");
            check(box->owned_element_count() == owner_count);
        }
    }

    it("retires old Box-owned Markdown projections across Unicode source swaps") {
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-c11-retire-231", "# 初回 😀\n");
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        const std::size_t owner_count = box->owned_element_count();
        auto* retired = static_cast<Element*>(md->children()[0]);
        box->set_focus(retired);
        box->set_mouse_capture(retired);

        for (std::size_t round = 0; round < 64u; ++round) {
            const bool odd = (round & 1u) != 0u;
            widget->set_markdown(odd ? "# 完成 ✅\n" : "# 更新 🏯\n");
            widget->update(0.0f, *md);
            check(md->child_count() == 1u);
            if (md->child_count() != 1u) return;
            auto* heading = static_cast<Element*>(md->children()[0]);
            check(heading->has_class(Symbol("md-header")));
            check(heading->text() == (odd ? "完成 ✅" : "更新 🏯"));
            check(box->owned_element_count() == owner_count);
            check(box->focused_element() == nullptr);
            check(box->capturing_element() == nullptr);
        }

        widget->set_markdown("");
        widget->update(0.0f, *md);
        check(md->child_count() == 0u);
        check(box->owned_element_count() + 1u == owner_count);
    }

    it("defers reentrant Markdown updates from focus-out during retirement") {
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-c11-reenter-231", "# 初期 📘\n");
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        const auto steady_count = box->owned_element_count();
        auto* first = static_cast<Element*>(md->children()[0]);
        box->set_focus(first);

        bool callback_seen = false;
        box->set_framework_event_callback(
            [&](Element&, const Event&) {
                if (callback_seen) return;
                callback_seen = true;
                widget->set_markdown("# 回调延后 ✅\n");
                // Must be rejected while the owner is retiring the old tree.
                widget->update(0.0f, *md);
            });
        widget->set_markdown("# 中间内容 📝\n");
        widget->update(0.0f, *md);
        box->set_framework_event_callback({});

        check(callback_seen);
        check(box->owned_element_count() == steady_count);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u)
            check(static_cast<Element*>(md->children()[0])->text() == "中间内容 📝");
        check(widget->markdown() == "# 回调延后 ✅\n");

        // A subsequent non-reentrant update consumes the deferred source.
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u)
            check(static_cast<Element*>(md->children()[0])->text() == "回调延后 ✅");
        check(box->owned_element_count() == steady_count);
    }

    it("drops retired Markdown style baselines through layout and Unicode swaps") {
        Box live(nullptr);
        auto* root = live.create("div", "md-style-cache-root-231");
        auto* host = live.create_widget<MarkdownWidget>(
            "div", "md-style-cache-host-231", "# 初期 😀\n");
        root->append(host);
        live.set_root(root);
        live.set_viewport(280.0f, 160.0f);
        live.load_css("#md-style-cache-host-231 { width: 200px; } "
                      ".md-header { color: #123456; }");
        auto* widget = static_cast<MarkdownWidget*>(host->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *host);
        live.update();
        const size_t initial_elements = live.owned_element_count();
        const size_t initial_baselines = live.cached_style_element_count();
        check(initial_baselines >= 3u);

        for (size_t round = 0; round < 48u; ++round) {
            const bool alternate = (round % 2u) != 0u;
            widget->set_markdown(
                alternate ? "# Ελληνικά café ✅\n" : "# 東京 🇯🇵\n");
            widget->update(0.0f, *host);
            live.update(); // Actually apply StyleEngine to each new subtree.
            check(host->child_count() == 1u);
            if (host->child_count() != 1u) return;
            auto* heading = static_cast<Element*>(host->child_at(0u));
            check(heading->has_class(Symbol("md-header")));
            check(heading->text() == (alternate ? "Ελληνικά café ✅" : "東京 🇯🇵"));
            check(live.owned_element_count() == initial_elements);
            check(live.cached_style_element_count() == initial_baselines);
        }
    }

    it("keeps Unicode GFM task/table roots through scrolling and layout changes") {
        Box live(nullptr);
        auto* root = live.create("div", "live-md-root-231");
        auto* scroll = live.create("div", "live-md-scroll-231");
        static const char* const sources[] = {
            "# 日本語 café 😀 Alpha\n\n"
            "- [x] 発送済み\n- [ ] 待機中\n\n"
            "| City | Price |\n| :--- | ---: |\n| 東京 | €20 |\n",
            "# Ελληνικά café 😀 Beta\n\n"
            "- [ ] review\n- [x] 完了\n\n"
            "| City | Price |\n| :--- | ---: |\n| Αθήνα | £30 |\n"
        };
        auto* host = live.create_widget<MarkdownWidget>(
            "div", "live-md-widget-231", sources[0]);
        auto* spacer = live.create("div", "live-md-spacer-231");
        root->append(scroll);
        scroll->append(host);
        scroll->append(spacer);
        live.set_root(root);
        live.set_viewport(320.0f, 180.0f);
        live.load_css(R"(
          #live-md-scroll-231 {
            overflow-y: scroll;
            width: 220px;
            height: 70px;
          }
          #live-md-widget-231 { width: 220px; }
          #live-md-spacer-231 { width: 220px; height: 280px; }
        )");
        auto* widget = static_cast<MarkdownWidget*>(host->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *host);
        live.update();
        check(scroll->can_scroll_y());
        if (!scroll->can_scroll_y()) return;
        const std::size_t steady_elements = live.owned_element_count();
        live.set_focus(scroll);

        for (std::size_t round = 0u; round < 10u; ++round) {
            const std::size_t selected = round % 2u;
            widget->set_markdown(sources[selected]);
            widget->update(0.0f, *host);
            live.set_viewport(round % 2u ? 260.0f : 320.0f, 180.0f);
            live.update();
            check(host->child_count() == 3u);
            if (host->child_count() != 3u) return;
            auto* heading = host->child_at(0u);
            auto* tasks = host->child_at(1u);
            auto* table = host->child_at(2u);
            check(heading->has_class(Symbol("md-header")));
            check(heading->text() == (selected ? "Ελληνικά café 😀 Beta" :
                                                "日本語 café 😀 Alpha"));
            check(tasks->has_class(Symbol("md-ul")));
            check(table->has_class(Symbol("md-table")));
            check(tasks->child_count() == 2u);
            check(table->child_count() == 2u);
            if (tasks->child_count() == 2u) {
                auto* task = tasks->child_at(0u);
                check(task->has_class(Symbol("md-task-item")));
                if (task->child_count()) {
                    auto* checkmark = task->child_at(0u);
                    check(checkmark->has_class(Symbol("md-checkbox")));
                    check(checkmark->text() == (selected ? "[ ] " : "[x] "));
                }
            }
            if (table->child_count() == 2u) {
                auto* header_row = table->child_at(0u);
                check(header_row->child_count() == 2u);
                if (header_row->child_count() == 2u) {
                    auto* price = header_row->child_at(1u);
                    check(price->has_class(Symbol("md-align-right")));
                    check(price->computed_style != nullptr);
                    if (price->computed_style)
                        check(price->computed_style->text_align == TextAlign::Right);
                }
            }
            Event end = Event::key_down(KeyCode::End);
            live.dispatch_event(end);
            check(end.handled);
            check(scroll->scroll_y() >= scroll->max_scroll_y() - 0.1f);
            live.update();
            Event home = Event::key_down(KeyCode::Home);
            live.dispatch_event(home);
            check(home.handled);
            check(scroll->scroll_y() <= 0.1f);
            live.update();
            check(live.owned_element_count() == steady_elements);
            check(live.focused_element() == scroll);
        }

        auto* visible_heading = host->child_at(0u);
        widget->set_markdown(std::string(1024u * 1024u, 'x'));
        widget->update(0.0f, *host);
        live.update();
        check(host->child_count() == 3u);
        if (host->child_count() == 3u)
            check(host->child_at(0u) == visible_heading);
        check(live.owned_element_count() == steady_elements);
    }

    it("renders fenced C11-owned Unicode code with optional syntax tokens") {
        HighlighterFactory::instance().register_highlighter(
            "stun-c11-fixture", std::make_unique<C11FixtureHighlighter>());
        HighlighterFactory::instance().register_highlighter(
            "stun-c11-invalid", std::make_unique<C11InvalidHighlighter>());
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-c11-highlight-231",
            "\x60\x60\x60stun-c11-fixture\nint value = 7;\nreturn café😀;\n\x60\x60\x60\n");
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        auto* code = md->child_at(0u);
        check(code->has_class(Symbol("md-code-block")));
        check(code->has_class(Symbol("lang-stun-c11-fixture")));
        check(code->child_count() >= 2u);
        if (code->child_count() >= 2u) {
            auto* a = code->child_at(0u);
            auto* b = code->child_at(1u);
            check(a->has_class(Symbol("md-code-line")));
            check(b->has_class(Symbol("md-code-line")));
            check(code_line_text(a) == "int value = 7;");
            check(code_line_text(b) == "return café😀;");
            bool keyword = false;
            for (auto* node : b->children()) {
                auto* span = static_cast<Element*>(node);
                if (span->has_class(Symbol("hl-keyword")) &&
                    span->text() == "return")
                    keyword = true;
            }
            check(keyword);
        }
        const auto stable_count = box->owned_element_count();
        // An out-of-bounds UTF-8 byte offset must never slice RichDoc text.
        widget->set_markdown(
            "\x60\x60\x60stun-c11-invalid\nreturn café😀;\n\x60\x60\x60\n");
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u) {
            code = md->child_at(0u);
            check(code->child_count() >= 1u);
            if (code->child_count() >= 1u) {
                auto* line = code->child_at(0u);
                check(code_line_text(line) == "return café😀;");
                bool keyword = false;
                for (auto* node : line->children())
                    keyword |= static_cast<Element*>(node)->has_class(
                        Symbol("hl-keyword"));
                check(!keyword);
            }
        }
        // Missing renderer highlighter means plain code, NOT a new parser.
        widget->set_markdown(
            "\x60\x60\x60stun-unknown-language\nreturn café😀;\n\x60\x60\x60\n");
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u) {
            code = md->child_at(0u);
            check(code->has_class(Symbol("md-code-block")));
            check(code->child_count() >= 1u);
            if (code->child_count() >= 1u)
                check(code_line_text(code->child_at(0u)) == "return café😀;");
        }
        // No persistence of C11's borrowed code text or metadata.
        widget->set_markdown("# 下一段 ✅\n");
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u)
            check(md->child_at(0u)->text() == "下一段 ✅");
        check(box->owned_element_count() <= stable_count);
    }

    it("bounds optional highlighter tokens and preserves UTF-8 code bytes") {
        auto& factory = HighlighterFactory::instance();
        factory.register_highlighter(
            "stun-c11-split", std::make_unique<C11Utf8SplitHighlighter>());
        factory.register_highlighter(
            "stun-c11-flood", std::make_unique<C11FloodHighlighter>());
        auto calls = std::make_shared<size_t>(0u);
        factory.register_highlighter(
            "stun-c11-counted", std::make_unique<C11CountingHighlighter>(calls));

        const std::string utf8_source =
            "\x60\x60\x60stun-c11-split\nreturn café😀;\n\x60\x60\x60\n";
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-c11-hl-limits-231", utf8_source);
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        auto* block = md->child_at(0u);
        check(block->child_count() >= 1u);
        if (block->child_count() == 0u) return;
        auto* line = block->child_at(0u);
        check(code_line_text(line) == "return café😀;");
        for (auto* node : line->children())
            check(!static_cast<Element*>(node)->has_class(Symbol("hl-keyword")));

        const std::string sequence(1200u, 'a');
        widget->set_markdown(
            "\x60\x60\x60stun-c11-flood\n" + sequence +
            "\n\x60\x60\x60\n");
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        block = md->child_at(0u);
        check(block->child_count() >= 1u);
        if (block->child_count() == 0u) return;
        line = block->child_at(0u);
        check(code_line_text(line) == sequence);
        check(line->child_count() == 1u);
        if (line->child_count() == 1u)
            check(!line->child_at(0u)->has_class(Symbol("hl-keyword")));

        widget->set_markdown(
            "\x60\x60\x60stun-c11-counted\nreturn café😀;\n\x60\x60\x60\n");
        widget->update(0.0f, *md);
        check(*calls == 1u);

        // C11 still parses the entire bounded code block. The optional
        // presentation plugin must not inspect >64 KiB of code in one call.
        const std::string long_line(64u * 1024u + 17u, 'z');
        widget->set_markdown(
            "\x60\x60\x60stun-c11-counted\n" + long_line +
            "\n\x60\x60\x60\n");
        widget->update(0.0f, *md);
        check(*calls == 1u);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u) {
            block = md->child_at(0u);
            check(block->child_count() >= 1u);
            if (block->child_count() >= 1u)
                check(code_line_text(block->child_at(0u)) == long_line);
        }
    }

    it("keeps deferred retirement bounded when FocusOut callbacks throw") {
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-c11-focus-throw-231", "# 初期 🌱\n");
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        if (!widget) return;
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) return;
        auto* old_focus = md->child_at(0u);
        const std::size_t steady_count = box->owned_element_count();
        box->set_focus(old_focus);
        std::size_t focus_out_attempts = 0u;
        box->set_framework_event_callback([&](Element&, const Event& event) {
            if (event.type == EventType::FocusOut) {
                ++focus_out_attempts;
                throw std::runtime_error("injected FocusOut callback failure");
            }
        });

        widget->set_markdown("# 可见内容 ✅\n");
        widget->update(0.0f, *md);
        check(focus_out_attempts == 1u);
        check(md->child_count() == 1u);
        if (md->child_count() != 1u) {
            box->set_framework_event_callback({});
            return;
        }
        auto* visible = md->child_at(0u);
        check(visible->text() == "可见内容 ✅");
        check(box->owned_element_count() == steady_count + 1u);

        // Even repeated source changes cannot admit another projection
        // while an old detached subtree is still held by Box.
        widget->set_markdown("# 延后处理 🏯\n");
        for (std::size_t n = 0u; n < 8u; ++n) {
            widget->update(0.0f, *md);
            check(md->child_count() == 1u);
            if (md->child_count() != 1u) break;
            check(md->child_at(0u) == visible);
            check(box->owned_element_count() == steady_count + 1u);
        }
        check(focus_out_attempts > 1u);

        box->set_framework_event_callback({});
        widget->update(0.0f, *md);
        check(md->child_count() == 1u);
        if (md->child_count() == 1u)
            check(md->child_at(0u)->text() == "延后处理 🏯");
        check(box->owned_element_count() == steady_count);
        check(box->focused_element() == nullptr);
    }

    // #186 / #231: repeated source updates must preserve the sole C11
    // parser and the existing Box-owned presentation lifecycle.
    it("GFM repeated task table heading and empty updates remain bounded") {
        static const char* const sources[] = {
            "| H | V |\n| --- | --- |\n| a | b |\n",
            "- [x] done\n- [ ] later\n",
            "# Heading\n",
            ""
        };
        auto* md = box->create_widget<MarkdownWidget>(
            "div", "md-cycle-186", sources[0]);
        root->append(md);
        auto* widget = static_cast<MarkdownWidget*>(md->widget);
        check(widget != nullptr);
        check(md->parent_elem() == root);
        for (size_t round = 0; round < 32; ++round) {
            const size_t kind = round % 4u;
            widget->set_markdown(sources[kind]);
            widget->update(0.0f, *md);
            check(widget->markdown() == sources[kind]);
            const size_t expected = kind == 3u ? 0u : 1u;
            check(md->child_count() == expected);
            if (expected && md->child_count() == 1u) {
                auto* child = static_cast<Element*>(md->children()[0]);
                if (kind == 0u) {
                    check(child->has_class(Symbol("md-table")));
                    check(child->child_count() == 2u);
                } else if (kind == 1u) {
                    check(child->has_class(Symbol("md-ul")));
                    check(child->child_count() == 2u);
                    if (child->child_count() == 2u) {
                        auto* a=static_cast<Element*>(child->children()[0]);
                        auto* b=static_cast<Element*>(child->children()[1]);
                        check(a->has_class(Symbol("md-task-item")));
                        check(b->has_class(Symbol("md-task-item")));
                    }
                } else {
                    check(child->has_class(Symbol("h1")));
                    check(child->text() == "Heading");
                }
            }
            // An identical input is a no-op, never a second AST publication.
            const auto* current = md->child_count()
                ? md->children()[0] : nullptr;
            widget->set_markdown(sources[kind]);
            widget->update(0.0f, *md);
            check(md->child_count() == expected);
            if (expected && md->child_count() == 1u)
                check(md->children()[0] == current);
            check(md->parent_elem() == root);
        }
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
