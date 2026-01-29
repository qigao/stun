/*
 * flexUI - MarkdownWidget
 *
 * Markdown renderer using md4c and flexUI element hierarchy.
 */

#ifndef FLEXUI_MARKDOWN_WIDGET_H
#define FLEXUI_MARKDOWN_WIDGET_H

#include "../widget.h"
#include <string>

namespace flexUI {

/**
 * MarkdownWidget - Markdown renderer
 * 
 * Takes a markdown string and populates the owner Element with 
 * child elements representing the markdown structure.
 */
class MarkdownWidget : public Widget {
public:
    explicit MarkdownWidget(const std::string& markdown = "");

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "MarkdownWidget"; }

    // Markdown access
    const std::string& markdown() const { return markdown_; }
    void set_markdown(const std::string& markdown);

private:
    void rebuild_elements(Element& elem);

    std::string markdown_;
    bool needs_rebuild_ = true;
};

} // namespace flexUI

#endif // FLEXUI_MARKDOWN_WIDGET_H
