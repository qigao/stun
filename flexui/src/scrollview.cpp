#include <flexui/scrollview.h>
#include <cssbox_internal.h>
#include <cssbox.h>
#include <algorithm>

namespace flexui {

ScrollView::ScrollView(cssboxRenderer* renderer, const std::string& id,
                       const ScrollViewStyle& style)
    : Widget(renderer, id, "div"), style_(style) {
    // Set overflow style for clipping
    setInlineStyle("overflow", "hidden");
}

float ScrollView::getMaxScrollY() const {
    auto* el = element();
    float visible_height = el->computed.height;
    return std::max(0.0f, content_height_ - visible_height);
}

float ScrollView::getScrollbarHeight() const {
    auto* el = element();
    float visible_height = el->computed.height;
    if (content_height_ <= 0 || content_height_ <= visible_height) {
        return visible_height;
    }
    return std::max(30.0f, visible_height * (visible_height / content_height_));
}

float ScrollView::getScrollbarY() const {
    auto* el = element();
    float visible_height = el->computed.height;
    float scrollbar_height = getScrollbarHeight();
    float max_scroll = getMaxScrollY();

    if (max_scroll <= 0) return el->computed.y;

    float scroll_ratio = scroll_y_ / max_scroll;
    float scrollbar_track = visible_height - scrollbar_height;
    return el->computed.y + scroll_ratio * scrollbar_track;
}

bool ScrollView::isInsideScrollbar(float x, float y) const {
    auto* el = element();
    float sb_x = el->computed.x + el->computed.width - style_.scrollbarWidth - 2;
    float sb_y = getScrollbarY();
    float sb_w = style_.scrollbarWidth;
    float sb_h = getScrollbarHeight();

    return x >= sb_x && x <= sb_x + sb_w && y >= sb_y && y <= sb_y + sb_h;
}

void ScrollView::setScrollY(float scrollY) {
    float max_scroll = getMaxScrollY();
    scroll_y_ = std::clamp(scrollY, 0.0f, max_scroll);

    // Sync with CSS element
    cssboxSetScroll(element(), 0.0f, scroll_y_);
}

void ScrollView::setContentHeight(float height) {
    content_height_ = height;

    // Sync with CSS element
    cssboxSetContentHeight(element(), height);

    // Re-clamp scroll position
    setScrollY(scroll_y_);
}

bool ScrollView::handleScroll(float x, float y, float deltaX, float deltaY) {
    auto* el = element();

    // Check if mouse is inside this widget
    if (x < el->computed.x || x > el->computed.x + el->computed.width ||
        y < el->computed.y || y > el->computed.y + el->computed.height) {
        return false;
    }

    // Only scroll if content is larger than visible area
    if (content_height_ <= el->computed.height) {
        return false;
    }

    setScrollY(scroll_y_ - deltaY);
    return true;
}

bool ScrollView::handleMouseDown(float x, float y) {
    if (isInsideScrollbar(x, y)) {
        scrollbar_dragging_ = true;
        drag_start_y_ = y;
        drag_start_scroll_ = scroll_y_;
        return true;
    }
    return false;
}

bool ScrollView::handleMouseMove(float x, float y) {
    if (scrollbar_dragging_) {
        auto* el = element();
        float visible_height = el->computed.height;
        float scrollbar_height = getScrollbarHeight();
        float scrollbar_track = visible_height - scrollbar_height;

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
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w <= 0 || h <= 0) return;

    // Draw scrollbar if content overflows
    if (style_.showScrollbar && content_height_ > h) {
        float sb_x = x + w - style_.scrollbarWidth - 2;
        float sb_y = getScrollbarY();
        float sb_h = getScrollbarHeight();

        // Scrollbar track (optional, subtle)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sb_x, y, style_.scrollbarWidth, h, style_.scrollbarRadius);
        nvgFillColor(vg, nvgRGBA(200, 200, 200, 50));
        nvgFill(vg);

        // Scrollbar thumb
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sb_x, sb_y, style_.scrollbarWidth, sb_h, style_.scrollbarRadius);
        nvgFillColor(vg, scrollbar_dragging_ ? style_.scrollbarHoverColor : style_.scrollbarColor);
        nvgFill(vg);
    }
}

} // namespace flexui
