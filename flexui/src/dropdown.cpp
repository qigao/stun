#include <flexui/dropdown.h>
#include <cssbox_internal.h>

namespace flexui {

// Helper to get accumulated scroll offset from parent chain
static void getScrollOffset(cssboxRenderer* renderer, cssboxElement* element,
                            float& scroll_x, float& scroll_y) {
    scroll_x = 0.0f;
    scroll_y = 0.0f;
    cssboxElement* parent = cssboxGetParent(renderer, element);
    while (parent) {
        scroll_x += parent->scroll_x;
        scroll_y += parent->scroll_y;
        parent = cssboxGetParent(renderer, parent);
    }
}

Dropdown::Dropdown(cssboxRenderer* renderer, const std::string& id,
                   const std::vector<std::string>& items, const DropdownStyle& style)
    : Widget(renderer, id, "select"), items_(items), style_(style) {
}

void Dropdown::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);
    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    // Main button background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Selected text
    if (selected_index_ >= 0 && selected_index_ < (int)items_.size()) {
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, textColor);
        nvgText(vg, x + style_.padding, y + h / 2, items_[selected_index_].c_str(), nullptr);
    }

    // Arrow (chevron down)
    float arrowX = x + w - 20;
    float arrowY = y + h / 2;
    nvgBeginPath(vg);
    if (open_) {
        // Chevron up when open
        nvgMoveTo(vg, arrowX - 5, arrowY + 2);
        nvgLineTo(vg, arrowX, arrowY - 3);
        nvgLineTo(vg, arrowX + 5, arrowY + 2);
    } else {
        // Chevron down when closed
        nvgMoveTo(vg, arrowX - 5, arrowY - 2);
        nvgLineTo(vg, arrowX, arrowY + 3);
        nvgLineTo(vg, arrowX + 5, arrowY - 2);
    }
    nvgStrokeColor(vg, style_.arrowColor);
    nvgStrokeWidth(vg, 2);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStroke(vg);

    // Dropdown list (rendered when open) - opens ABOVE the button
    if (open_ && !items_.empty()) {
        float listHeight = items_.size() * style_.itemHeight;
        float listY = y - listHeight - 2;  // Position above the button

        // List background with shadow
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, listY, w, listHeight, borderRadius);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);
        nvgStrokeColor(vg, style_.borderColor);
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);

        // Items
        for (size_t i = 0; i < items_.size(); i++) {
            float itemY = listY + i * style_.itemHeight;

            // Hover background
            if ((int)i == hover_index_) {
                nvgBeginPath(vg);
                bool isFirstOrLast = (i == 0) || (i == items_.size() - 1);
                if (isFirstOrLast) {
                    nvgRoundedRect(vg, x, itemY, w, style_.itemHeight, borderRadius);
                } else {
                    nvgRect(vg, x, itemY, w, style_.itemHeight);
                }
                nvgFillColor(vg, style_.bgColorHover);
                nvgFill(vg);
            }

            // Item text
            nvgFontSize(vg, fontSize);
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, textColor);
            nvgText(vg, x + style_.padding, itemY + style_.itemHeight / 2, items_[i].c_str(), nullptr);
        }
    }
}

bool Dropdown::handleMouseDown(float mx, float my) {
    auto* el = element();

    // Get scroll offset to convert layout coords to visual coords
    float scroll_x, scroll_y;
    getScrollOffset(renderer(), el, scroll_x, scroll_y);

    // Visual position (where widget appears on screen)
    float x = el->layout.x - scroll_x;
    float y = el->layout.y - scroll_y;
    float w = el->layout.width;
    float h = el->layout.height;

    // Check main button
    if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
        open_ = !open_;
        return true;
    }

    // Check dropdown items when open - list is ABOVE the button
    if (open_ && !items_.empty()) {
        float listHeight = items_.size() * style_.itemHeight;
        float listY = y - listHeight - 2;  // Position above the button

        if (mx >= x && mx <= x + w && my >= listY && my <= listY + listHeight) {
            int index = (int)((my - listY) / style_.itemHeight);
            if (index >= 0 && index < (int)items_.size()) {
                setSelectedIndex(index);
                open_ = false;
                return true;
            }
        }

        // Click outside closes dropdown
        open_ = false;
    }

    return false;
}

bool Dropdown::handleMouseMove(float mx, float my) {
    if (!open_) {
        hover_index_ = -1;
        return false;
    }

    auto* el = element();

    // Get scroll offset to convert layout coords to visual coords
    float scroll_x, scroll_y;
    getScrollOffset(renderer(), el, scroll_x, scroll_y);

    // Visual position (where widget appears on screen)
    float x = el->layout.x - scroll_x;
    float y = el->layout.y - scroll_y;
    float w = el->layout.width;

    float listHeight = items_.size() * style_.itemHeight;
    float listY = y - listHeight - 2;  // Position above the button

    hover_index_ = -1;
    if (mx >= x && mx <= x + w && my >= listY && my <= listY + listHeight) {
        int index = (int)((my - listY) / style_.itemHeight);
        if (index >= 0 && index < (int)items_.size()) {
            hover_index_ = index;
            return true;
        }
    }

    return false;
}

void Dropdown::setSelectedIndex(int index) {
    if (index >= 0 && index < (int)items_.size()) {
        selected_index_ = index;
        if (change_callback_) {
            change_callback_(index, items_[index]);
        }
    }
}

std::string Dropdown::getSelectedItem() const {
    if (selected_index_ >= 0 && selected_index_ < (int)items_.size()) {
        return items_[selected_index_];
    }
    return "";
}

} // namespace flexui
