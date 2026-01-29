#include <catch2/catch_all.hpp>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flex/bridge/renderer.h>

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
