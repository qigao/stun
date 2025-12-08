#include <flexui/scrollview.h>
#include <cssbox_internal.h>
#include <cssbox.h>
#include <algorithm>

namespace flexui {

// Recursively invalidate abs_transform for element and all descendants
static void invalidateTransformTree(cssboxRenderer* renderer, cssboxElement* element) {
    element->abs_transform_valid_ = false;
    for (int child_id : element->children_internal_ids) {
        auto it = renderer->elements.find(child_id);
        if (it != renderer->elements.end()) {
            invalidateTransformTree(renderer, it->second.get());
        }
    }
}

ScrollView::ScrollView(cssboxRenderer* renderer, const std::string& id,
                       const ScrollViewStyle& style)
    : Widget(renderer, id, "div"), style_(style) {
    setInlineStyle("overflow", "hidden");
}

float ScrollView::getMaxScrollY() const {
    return std::max(0.0f, content_height_ - viewport_h_);
}

float ScrollView::getScrollbarHeight() const {
    if (content_height_ <= 0 || content_height_ <= viewport_h_) {
        return viewport_h_;
    }
    return std::max(30.0f, viewport_h_ * (viewport_h_ / content_height_));
}

float ScrollView::getScrollbarY() const {
    float max_scroll = getMaxScrollY();
    if (max_scroll <= 0) return 0.0f;

    float scrollbar_height = getScrollbarHeight();
    float scrollbar_track = viewport_h_ - scrollbar_height;
    float scroll_ratio = scroll_y_ / max_scroll;
    return scroll_ratio * scrollbar_track;
}

void ScrollView::setViewportSize(float w, float h) {
    if (viewport_w_ == w && viewport_h_ == h) return;
    viewport_w_ = w;
    viewport_h_ = h;
    // Update element CSS dimensions to match viewport
    setInlineStyle("width", std::to_string((int)w) + "px");
    setInlineStyle("height", std::to_string((int)h) + "px");
    // Reclamp scroll position in case max scroll changed
    setScrollY(scroll_y_);
}

void ScrollView::setContentHeight(float height) {
    if (content_height_ == height) return;
    content_height_ = height;
    // Force spatial index rebuild when content height changes
    renderer()->layout_dirty = true;
    // Reclamp scroll position in case max scroll changed
    setScrollY(scroll_y_);
}

void ScrollView::setScrollY(float scrollY) {
    float max_scroll = getMaxScrollY();
    float new_scroll = std::clamp(scrollY, 0.0f, max_scroll);
    
    // Only update if scroll position actually changed
    if (new_scroll == scroll_y_) return;
    
    scroll_y_ = new_scroll;
    cssboxSetScroll(element(), 0.0f, scroll_y_);

    // Invalidate all descendant transforms and trigger repaint
    invalidateTransformTree(renderer(), element());
    renderer()->paint_dirty_ = true;
    // Mark layout dirty to rebuild spatial index with new scroll positions
    renderer()->layout_dirty = true;
}

bool ScrollView::handleScroll(float x, float y, float deltaX, float deltaY) {
    // Simple bounds check using viewport size
    if (viewport_w_ <= 0 || viewport_h_ <= 0) return false;
    if (content_height_ <= viewport_h_) return false;

    setScrollY(scroll_y_ - deltaY);
    return true;
}

bool ScrollView::handleMouseDown(float x, float y) {
    float base_x, base_y;
    getVisualPosition(base_x, base_y);

    // Scrollbar hit test
    float sb_x = base_x + viewport_w_ - style_.scrollbarWidth - 2;
    float sb_y = base_y + getScrollbarY();
    float sb_w = style_.scrollbarWidth;
    float sb_h = getScrollbarHeight();

    if (x >= sb_x && x <= sb_x + sb_w && y >= sb_y && y <= sb_y + sb_h) {
        scrollbar_dragging_ = true;
        drag_start_y_ = y;
        drag_start_scroll_ = scroll_y_;
        return true;
    }
    return false;
}

bool ScrollView::handleMouseMove(float x, float y) {
    if (scrollbar_dragging_) {
        float scrollbar_height = getScrollbarHeight();
        float scrollbar_track = viewport_h_ - scrollbar_height;

        if (scrollbar_track > 0) {
            float delta_y = y - drag_start_y_;
            float scroll_delta = (delta_y / scrollbar_track) * getMaxScrollY();
            setScrollY(drag_start_scroll_ + scroll_delta);
        }
        return true;
    }
    return false;
}

bool ScrollView::handleMouseUp(float x, float y) {
    if (scrollbar_dragging_) {
        scrollbar_dragging_ = false;
        return true;
    }
    return false;
}

void ScrollView::draw(NVGcontext* vg) {
    if (viewport_w_ <= 0 || viewport_h_ <= 0) return;
    if (content_height_ <= viewport_h_) return;  // No scrollbar needed
    if (!style_.showScrollbar) return;

    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;

    float sb_x = x + viewport_w_ - style_.scrollbarWidth - 2;
    float sb_y = y + getScrollbarY();
    float sb_h = getScrollbarHeight();

    // Scrollbar track
    nvgBeginPath(vg);
    nvgRoundedRect(vg, sb_x, y, style_.scrollbarWidth, viewport_h_, style_.scrollbarRadius);
    nvgFillColor(vg, nvgRGBA(200, 200, 200, 50));
    nvgFill(vg);

    // Scrollbar thumb
    nvgBeginPath(vg);
    nvgRoundedRect(vg, sb_x, sb_y, style_.scrollbarWidth, sb_h, style_.scrollbarRadius);
    nvgFillColor(vg, scrollbar_dragging_ ? style_.scrollbarHoverColor : style_.scrollbarColor);
    nvgFill(vg);
}

} // namespace flexui
