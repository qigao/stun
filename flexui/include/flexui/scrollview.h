#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <algorithm>

namespace flexui {

/**
 * ScrollView - A scrollable container widget
 *
 * Current implementation:
 * - Handles mouse wheel scroll events
 * - Displays a scrollbar
 * - Tracks scroll offset
 *
 * Limitations (TODO):
 * - Visual content scrolling not yet implemented (requires cssbox changes)
 * - For now, use larger window sizes or paginated layouts to handle overflow
 * - setContentHeight() must be called manually to enable scrolling
 *
 * Usage:
 *   <scrollview id="my-scroll" style="width: 300px; height: 400px;">
 *       <!-- children -->
 *   </scrollview>
 */

struct ScrollViewStyle {
    NVGcolor scrollbarColor = nvgRGBA(128, 128, 128, 100);
    NVGcolor scrollbarHoverColor = nvgRGBA(128, 128, 128, 180);
    float scrollbarWidth = 8.0f;
    float scrollbarRadius = 4.0f;
    bool showScrollbar = true;
};

class ScrollView : public Widget {
public:
    ScrollView(cssboxRenderer* renderer, const std::string& id,
               const ScrollViewStyle& style = ScrollViewStyle());

    void draw(NVGcontext* vg) override;
    bool handleScroll(float x, float y, float deltaX, float deltaY) override;
    bool handleMouseDown(float x, float y) override;
    bool handleMouseMove(float x, float y) override;
    bool handleMouseUp(float x, float y) override;

    void setContentHeight(float height);
    float getContentHeight() const { return content_height_; }

    void setScrollY(float scrollY);
    float getScrollY() const { return scroll_y_; }

    void setScrollViewStyle(const ScrollViewStyle& style) { style_ = style; }

    // Get scroll offset for coordinate adjustment
    float getScrollOffsetY() const { return scroll_y_; }

private:
    float scroll_y_ = 0.0f;
    float content_height_ = 0.0f;  // Total content height
    ScrollViewStyle style_;
    bool scrollbar_dragging_ = false;
    float drag_start_y_ = 0.0f;
    float drag_start_scroll_ = 0.0f;

    float getMaxScrollY() const;
    float getScrollbarHeight() const;
    float getScrollbarY() const;
    bool isInsideScrollbar(float x, float y) const;
};

} // namespace flexui
