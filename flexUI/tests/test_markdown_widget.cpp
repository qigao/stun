#include <tinytest.hpp>
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

        // Existing widget update, not a C11 parser cutover: old children
        // must no longer be visible, and repeated parsing releases its
        // arena-allocated C++ AST via the single ParseContext destructor.
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

    // #186/#229/#230: the root TABLE after a blank-only task list must be
    // a real sibling of LIST and HEADING. The shipping C++ parser failed
    // this exact markup; MarkdownWidget now projects the new C11 RichDoc.
    // No fake '---' separator, private Unicode table or C++ grammar fallback.
    it("projects C11 Unicode task list then root table after blank only") {
        static const char* const sources[] = {
            "# 日本語 café 😀 Alpha\n\n"
            "- [x] 発送済み\n- [ ] 待機中\n\n"
            "| 城市 | Price |\n| :--- | ---: |\n| 東京 | €20 |\n",
            "# Ελληνικά café 😀 Beta\n\n"
            "- [ ] review\n- [x] 完了\n\n"
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
            check(md->child_count() == 3u);
            if (md->child_count() != 3u) return;

            auto* heading = static_cast<Element*>(md->children()[0]);
            auto* list = static_cast<Element*>(md->children()[1]);
            auto* table = static_cast<Element*>(md->children()[2]);
            check(heading->has_class(Symbol("md-header")));
            check(heading->has_class(Symbol("h1")));
            check(heading->text() == headings[index]);
            check(list->has_class(Symbol("md-ul")));
            check(list->child_count() == 2u);
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
            // These semantics are copied from the *C11 RichDoc* delimiter
            // metadata vector; no renderer reparses colon alignment.
            if (table->child_count() == 2u) {
                auto* header = static_cast<Element*>(table->children()[0]);
                if (header->child_count() == 2u) {
                    auto* left = static_cast<Element*>(header->children()[0]);
                    auto* right = static_cast<Element*>(header->children()[1]);
                    check(left->has_class(Symbol("md-align-left")));
                    check(right->has_class(Symbol("md-align-right")));
                    const std::string* left_align =
                        left->attribute("data-md-align");
                    const std::string* right_align =
                        right->attribute("data-md-align");
                    check(left_align != nullptr);
                    check(right_align != nullptr);
                    if (left_align) check(*left_align == "left");
                    if (right_align) check(*right_align == "right");
                }
            }
            const Element* const original_heading = heading;
            const Element* const original_list = list;
            const Element* const original_table = table;
            widget->set_markdown(sources[index]);
            widget->update(0.0f, *md);
            check(md->child_count() == 3u);
            if (md->child_count() == 3u) {
                check(md->children()[0] == original_heading);
                check(md->children()[1] == original_list);
                check(md->children()[2] == original_table);
            }
            check(md->parent_elem() == root);
        }
    }

    // #230: the *new C11* grammar owns all live MarkdownWidget updates;
    // exercise task/table/empty transitions with no second parser or owner.
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

    // #230: a source that exceeds the single-owner C11 source budget
    // cannot retain stale visual state, publish a partial tree or invoke
    // legacy md::parse. A subsequent accepted source must recover.
    it("fails closed on over-budget C11 source and recovers without fallback") {
        auto* md=box->create_widget<MarkdownWidget>(
            "div","md-budget-fail-230","# Before\n");
        root->append(md);
        auto* widget=static_cast<MarkdownWidget*>(md->widget);
        check(widget!=nullptr);
        if(widget==nullptr)return;
        widget->update(0.0f,*md);
        check(md->child_count()==1u);
        const std::string oversized(1024u*1024u+1u,'a');
        widget->set_markdown(oversized);
        widget->update(0.0f,*md);
        check(widget->markdown()==oversized);
        check(md->child_count()==0u);
        widget->set_markdown("# After\n");
        widget->update(0.0f,*md);
        check(md->child_count()==1u);
        if(md->child_count()==1u) {
            auto* heading=static_cast<Element*>(md->children()[0]);
            check(heading->has_class(Symbol("md-header")));
            check(heading->text()=="After");
        }
        check(md->parent_elem()==root);
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
