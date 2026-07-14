/*
 * flexUI - MarkdownWidget Implementation
 */

#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/syntax_highlighter.h>
#include <flexUI/element.h>
#include <flexUI/box.h>
#include <md.h>
#include <vector>
#include <iostream>

namespace flexUI {

/**
 * @brief Helper to convert a md AST node down to flexUI elements.
 */
static void convert_node(md::Node* node, Element* parent, Box* box) {
    if (!node) return;

    Element* elem = nullptr;
    bool process_children = true;

    switch (node->type) {
        case md::NodeType::Document:
            // Document is just a container, process children directly onto the parent element
            for (auto child : node->children) {
                convert_node(child, parent, box);
            }
            return;

        case md::NodeType::Paragraph:
            elem = box->create("div");
            elem->add_class("md-p");
            // If paragraph has only one Text child, set text directly
            if (node->children.size() == 1 && 
                node->children[0]->type == md::NodeType::Text) {
                elem->set_text(node->children[0]->text);
                process_children = false;
            }
            break;

        case md::NodeType::Header: {
            char tag[4];
            snprintf(tag, sizeof(tag), "h%d", node->level);
            elem = box->create(tag);
            elem->add_class("md-header");
            elem->add_class(tag);
            // If header has only one Text child, set text directly
            if (node->children.size() == 1 && 
                node->children[0]->type == md::NodeType::Text) {
                elem->set_text(node->children[0]->text);
                process_children = false;
            }
            break;
        }

        case md::NodeType::List:
            elem = box->create("div");
            elem->add_class("md-ul");
            break;

        case md::NodeType::OrderedList:
            elem = box->create("div");
            elem->add_class("md-ol");
            break;

        case md::NodeType::ListItem:
        case md::NodeType::TaskItem:
            elem = box->create("div");
            elem->add_class("md-li");
            if (node->type == md::NodeType::TaskItem) {
                elem->add_class("md-task-item");
            }
            break;

        case md::NodeType::Text:
        case md::NodeType::HtmlEntity:
            if (!node->text.empty()) {
                elem = box->create("span");
                elem->set_text(node->text);
            }
            break;

        case md::NodeType::Emphasis:
            elem = box->create("span");
            elem->add_class("md-em");
            // If only one Text child, set text directly
            if (node->children.size() == 1 && 
                node->children[0]->type == md::NodeType::Text) {
                elem->set_text(node->children[0]->text);
                process_children = false;
            }
            break;

        case md::NodeType::Strong:
            elem = box->create("span");
            elem->add_class("md-strong");
            // If only one Text child, set text directly
            if (node->children.size() == 1 && 
                node->children[0]->type == md::NodeType::Text) {
                elem->set_text(node->children[0]->text);
                process_children = false;
            }
            break;

        case md::NodeType::Link:
            elem = box->create("span");
            elem->add_class("md-a");
            // We could store the link URL in a custom attribute if flexUI supports it
            break;

        case md::NodeType::Image:
            elem = box->create("span");
            elem->add_class("md-img");
            break;

        case md::NodeType::CodeBlock: {
            elem = box->create("div");
            elem->add_class("md-code-block");
            
            if (!node->language.empty()) {
                elem->add_class("lang-" + node->language);
            }
            
            auto* highlighter = HighlighterFactory::instance().get(node->language);
            
            for (auto* child : node->children) {
                if (child->type != md::NodeType::Text || child->text.empty()) continue;
                
                std::string_view content = child->text;
                
                if (highlighter) {
                    auto tokens = highlighter->highlight(content);
                    auto* line = box->create("div");
                    line->add_class("md-code-line");
                    
                    for (const auto& tok : tokens) {
                        std::string_view text = content.substr(tok.start, tok.length);
                        size_t pos = 0;
                        while (pos < text.size()) {
                            size_t nl = text.find('\n', pos);
                            if (nl == std::string_view::npos) nl = text.size();
                            
                            if (nl > pos) {
                                auto* span = box->create("span");
                                span->add_class(highlight_class(tok.type));
                                span->set_text(std::string(text.substr(pos, nl - pos)));
                                line->append(span);
                            }
                            
                            if (nl < text.size()) {
                                elem->append(line);
                                line = box->create("div");
                                line->add_class("md-code-line");
                            }
                            pos = nl + 1;
                        }
                    }
                    if (!line->children().empty() || elem->children().empty()) {
                        elem->append(line);
                    }
                } else {
                    size_t start = 0;
                    while (start < content.size()) {
                        size_t end = content.find('\n', start);
                        if (end == std::string_view::npos) end = content.size();
                        auto* line = box->create("div");
                        line->add_class("md-code-line");
                        line->set_text(std::string(content.substr(start, end - start)));
                        elem->append(line);
                        start = end + 1;
                    }
                }
            }
            process_children = false;
            break;
        }

        case md::NodeType::CodeSpan:
            elem = box->create("span");
            elem->add_class("md-code");
            break;

        case md::NodeType::Table:
            elem = box->create("div");
            elem->add_class("md-table");
            break;

        case md::NodeType::TableRow:
            elem = box->create("div");
            elem->add_class("md-tr");
            break;

        case md::NodeType::TableCell:
            elem = box->create("div");
            elem->add_class("md-td");
            break;

        case md::NodeType::Strikethrough:
            elem = box->create("span");
            elem->add_class("md-del");
            break;

        case md::NodeType::Underline:
            elem = box->create("span");
            elem->add_class("md-u");
            break;

        case md::NodeType::HorizontalRule:
            elem = box->create("div");
            elem->add_class("md-hr");
            break;

        case md::NodeType::BlockQuote:
            elem = box->create("div");
            elem->add_class("md-quote");
            break;

        case md::NodeType::LineBreak:
            elem = box->create("div"); // Simple spacer
            elem->add_class("md-br");
            break;

        case md::NodeType::Checkbox:
            elem = box->create("span");
            elem->add_class("md-checkbox");
            elem->set_text(node->text == "x" ? "[x] " : "[ ] ");
            process_children = false; 
            break;


        default:
            elem = nullptr;
            break;
    }

    if (elem) {
        if (!elem->parent()) parent->append(elem);
        if (process_children) {
            for (auto child : node->children) {
                convert_node(child, elem, box);
            }
        }
    } else {
        // Container nodes or text nodes appened directly to parent
        for (auto child : node->children) {
            convert_node(child, parent, box);
        }
    }
}

MarkdownWidget::MarkdownWidget(const std::string& markdown)
    : markdown_(markdown) {}

void MarkdownWidget::set_markdown(const std::string& markdown) {
    if (markdown_ == markdown) return;
    markdown_ = markdown;
    needs_rebuild_ = true;
}

void MarkdownWidget::rebuild_elements(Element& elem) {
    if (!needs_rebuild_) return;
    if (!elem.owner_box_) return;

    // Ensure it's a vertical container
    if (elem.computed_style) {
        elem.computed_style->flex_direction = FlexDirection::Column;
    }

    // Clear existing children
    elem.clear_children();
    elem.set_text(""); // Also clear any direct text
    
    // Parse the markdown using the new arena-allocated parser
    auto result = md::parse(markdown_);
    
    if (result.root) {
        // Convert the AST to flexUI elements
        convert_node(result.root, &elem, elem.owner_box_);
    }
    
    needs_rebuild_ = false;
    elem.mark_style_dirty(); // Force style and layout update for the new children
}

void MarkdownWidget::emit_render_commands(const Element& elem,
                                          RenderCommandList& commands) {
    (void)elem;
    (void)commands;
    // The actual rendering is handled by the Element::render for each child.
}

bool MarkdownWidget::handle_event(const Event& event, Element& elem) {
    return false;
}

void MarkdownWidget::update(float delta_ms, Element& elem) {
    // Rebuild elements here so they are ready for the style/layout pass
    rebuild_elements(elem);
}

} // namespace flexUI
