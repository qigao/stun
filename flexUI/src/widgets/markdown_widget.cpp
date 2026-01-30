/*
 * flexUI - MarkdownWidget Implementation
 */

#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/element.h>
#include <flexUI/box.h>
#include <md_re2c.h>
#include <vector>
#include <iostream>

namespace flexUI {

/**
 * @brief Helper to convert a md AST node down to flexUI elements.
 */
static void convert_node(md_re2c::Node* node, Element* parent, Box* box) {
    if (!node) return;

    Element* elem = nullptr;
    bool process_children = true;

    switch (node->type) {
        case md_re2c::NodeType::Document:
            // Document is just a container, process children directly onto the parent element
            for (auto child : node->children) {
                convert_node(child, parent, box);
            }
            return;

        case md_re2c::NodeType::Paragraph:
            elem = box->create("div");
            elem->add_class("md-p");
            break;

        case md_re2c::NodeType::Header: {
            char tag[4];
            snprintf(tag, sizeof(tag), "h%d", node->level);
            elem = box->create(tag);
            elem->add_class("md-header");
            elem->add_class(tag);
            break;
        }

        case md_re2c::NodeType::List:
            elem = box->create("div");
            elem->add_class("md-ul");
            break;

        case md_re2c::NodeType::OrderedList:
            elem = box->create("div");
            elem->add_class("md-ol");
            break;

        case md_re2c::NodeType::ListItem:
        case md_re2c::NodeType::TaskItem:
            elem = box->create("div");
            elem->add_class("md-li");
            if (node->type == md_re2c::NodeType::TaskItem) {
                elem->add_class("md-task-item");
            }
            break;

        case md_re2c::NodeType::Text:
        case md_re2c::NodeType::HtmlEntity:
            if (!node->text.empty()) {
                elem = box->create("span");
                elem->set_text(node->text);
            }
            break;

        case md_re2c::NodeType::Emphasis:
            elem = box->create("span");
            elem->add_class("md-em");
            break;

        case md_re2c::NodeType::Strong:
            elem = box->create("span");
            elem->add_class("md-strong");
            break;

        case md_re2c::NodeType::Link:
            elem = box->create("span");
            elem->add_class("md-a");
            // We could store the link URL in a custom attribute if flexUI supports it
            break;

        case md_re2c::NodeType::Image:
            elem = box->create("span");
            elem->add_class("md-img");
            break;

        case md_re2c::NodeType::CodeBlock:
            elem = box->create("div");
            elem->add_class("md-code-block");
            break;

        case md_re2c::NodeType::CodeSpan:
            elem = box->create("span");
            elem->add_class("md-code");
            break;

        case md_re2c::NodeType::Table:
            elem = box->create("div");
            elem->add_class("md-table");
            break;

        case md_re2c::NodeType::TableRow:
            elem = box->create("div");
            elem->add_class("md-tr");
            break;

        case md_re2c::NodeType::TableCell:
            elem = box->create("div");
            elem->add_class("md-td");
            break;

        case md_re2c::NodeType::Strikethrough:
            elem = box->create("span");
            elem->add_class("md-del");
            break;

        case md_re2c::NodeType::Underline:
            elem = box->create("span");
            elem->add_class("md-u");
            break;

        case md_re2c::NodeType::HorizontalRule:
            elem = box->create("div");
            elem->add_class("md-hr");
            break;

        case md_re2c::NodeType::LineBreak:
            elem = box->create("div"); // Simple spacer
            elem->add_class("md-br");
            break;

        case md_re2c::NodeType::Checkbox:
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
    auto result = md_re2c::parse(markdown_);
    
    if (result.root) {
        // Convert the AST to flexUI elements
        convert_node(result.root, &elem, elem.owner_box_);
    }
    
    needs_rebuild_ = false;
    elem.mark_style_dirty(); // Force style and layout update for the new children
}

void MarkdownWidget::render(const Element& elem, Renderer& renderer) {
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
