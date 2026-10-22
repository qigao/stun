/*
 * flexUI - MarkdownWidget
 *
 * Markdown renderer using the sole Stun C11 RichDoc parser and owned FlexUI elements.
 */

#ifndef FLEXUI_MARKDOWN_WIDGET_H
#define FLEXUI_MARKDOWN_WIDGET_H

#include "../widget.h"
#include <cstddef>
#include <string>
#include <vector>

namespace flexUI {

/**
 * MarkdownWidget - Markdown renderer
 * 
 * Takes a markdown string and populates the owner Element with 
 * child elements representing the markdown structure.
 */
class MarkdownWidget : public Widget {
public:
    static constexpr std::size_t kDefaultMaxRenderElements = 16384u;
    explicit MarkdownWidget(
        const std::string& markdown = "",
        std::size_t max_render_elements = kDefaultMaxRenderElements);

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
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
    bool rebuilding_ = false;
    const std::size_t max_render_elements_;
    // Borrowed identities only; Box remains the sole Element owner.
    std::vector<Element*> published_projection_;
    // At most one detached projection may await a callback-safe Box drain.
    // If retirement fails, further parse/publish work is blocked until it
    // succeeds; the renderer cannot accumulate unbounded orphaned Elements.
    std::vector<Element*> pending_retirement_;
};

} // namespace flexUI

#endif // FLEXUI_MARKDOWN_WIDGET_H
