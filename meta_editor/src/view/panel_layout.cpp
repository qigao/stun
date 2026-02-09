/*
 * Meta Editor - Panel Layout Implementation
 */

#include "meta_editor/view/panel_layout.h"

namespace meta_editor {

void PanelLayout::add_panel(Panel* panel, DockSide side) {
    if (panel) entries_.push_back({panel, side});
}

void PanelLayout::set_viewport(float width, float height) {
    viewport_w_ = width;
    viewport_h_ = height;
}

void PanelLayout::layout() {
    float left_y = margin_;
    float right_y = margin_;

    for (auto& entry : entries_) {
        if (!entry.panel->is_visible()) continue;

        float w = entry.panel->width();
        float h = entry.panel->content_height();

        switch (entry.side) {
            case DockSide::Left:
                entry.panel->set_position(margin_, left_y);
                left_y += h + gap_;
                break;
            case DockSide::Right:
                entry.panel->set_position(viewport_w_ - w - margin_, right_y);
                right_y += h + gap_;
                break;
            case DockSide::Bottom:
                entry.panel->set_position(viewport_w_ - w - margin_,
                                          viewport_h_ - h - margin_);
                break;
        }
    }
}

} // namespace meta_editor
