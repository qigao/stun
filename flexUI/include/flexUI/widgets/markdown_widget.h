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
#include <string_view>
#include <vector>

namespace flexUI {

enum class MarkdownAppendStatus {
    Appended,
    NoChange,
    CapacityExceeded
};

/**
 * MarkdownWidget - Markdown renderer
 * 
 * Takes a markdown string and populates the owner Element with 
 * child elements representing the markdown structure.
 */
class MarkdownWidget : public Widget {
public:
    static constexpr std::size_t kDefaultMaxRenderElements = 16384u;
    static constexpr std::size_t kMaxMarkdownBytes = 1024u * 1024u;
    explicit MarkdownWidget(
        const std::string& markdown = "",
        std::size_t max_render_elements = kDefaultMaxRenderElements);

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "MarkdownWidget"; }
    WidgetLocalBorrowSnapshot inspect_local_borrowers() const noexcept override {
      // A parsed projection is Box-owned but Markdown keeps borrowed raw
      // identities, including a possibly DETACHED deferred retirement batch.
      // Surface both even when still unaudited: the future physical owner
      // protocol must drain them without parser/frame callbacks.
      return {false, 0,
              needs_rebuild_ || rebuilding_ ||
                  !published_projection_.empty() || !pending_retirement_.empty(),
              published_projection_.size(), pending_retirement_.size()};
    }

    // Markdown access
    const std::string& markdown() const { return markdown_; }
    void set_markdown(const std::string& markdown);

    // UI-owner-thread only. Appends copied source bytes without parsing now;
    // multiple calls coalesce into one projection at the next update(). The
    // source is bounded, and a capacity rejection leaves it unchanged.
    MarkdownAppendStatus append_markdown(std::string_view delta);

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
