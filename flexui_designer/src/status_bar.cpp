/*
 * flexUI Designer - Status Bar Implementation
 */

#include "flexui_designer/status_bar.h"
#include "flexui_designer/designer.h"
#include "flexui_designer/design_canvas.h"
#include <cstdio>

namespace flexui_designer {

void StatusBar::render(flex::Renderer& renderer) {
    renderer.save();
    renderer.translate(x_, y_);

    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.12f, 0.12f, 0.14f, 1});
    flex::Paint border = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.28f, 1});
    renderer.draw_rect(0, 0, width_, height_, 0, bg, flex::Paint::none(), 0);
    renderer.draw_rect(0, 0, width_, 1, 0, border, flex::Paint::none(), 0);

    float text_y = 11; // Adjusted for centering 11px font in bar
    flex::Color text_col{0.7f, 0.7f, 0.7f, 1};
    flex::Color highlight{0.9f, 0.9f, 0.9f, 1};

    // Left: selection info
    if (designer_) {
        auto* w = designer_->selected_widget();
        if (w) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s  |  X: %.0f  Y: %.0f  |  W: %.0f  H: %.0f",
                w->id.c_str(), w->x, w->y, w->width, w->height);
            renderer.draw_text(buf, 10, text_y, "sans", 11, false, highlight);
        } else {
            size_t count = designer_->selection().size();
            if (count > 1) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%zu widgets selected", count);
                renderer.draw_text(buf, 10, text_y, "sans", 11, false, text_col);
            } else {
                renderer.draw_text("No selection", 10, text_y, "sans", 11, false, text_col);
            }
        }

        // Center: message (temporary)
        if (message_time_ > 0 && !message_.empty()) {
            float msg_x = width_ / 2 - message_.length() * 3;
            flex::Color msg_col{0.4f, 0.8f, 0.5f, 1};
            renderer.draw_text(message_, msg_x, text_y, "sans", 11, false, msg_col);
        }

        // Right: zoom + widget count + grid/snap status
        auto* canvas = designer_->canvas();
        char right_buf[80];
        snprintf(right_buf, sizeof(right_buf), "%.0f%%  |  Widgets: %zu  |  Grid: %s  Snap: %s",
            canvas ? canvas->zoom() * 100 : 100.0f,
            (size_t)designer_->widgets().size(),
            canvas && canvas->show_grid() ? "ON" : "OFF",
            canvas && canvas->snap_enabled() ? "ON" : "OFF");
        float right_x = width_ - 260;
        renderer.draw_text(right_buf, right_x, text_y, "sans", 11, false, text_col);
    }

    renderer.restore();
}

} // namespace flexui_designer
