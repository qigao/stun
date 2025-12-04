#include <flexui/dropdown.h>
#include <cssbox_internal.h>

namespace flexui {

Dropdown::Dropdown(cssboxRenderer* renderer, const std::string& id,
                   const std::vector<std::string>& items, const DropdownStyle& style)
    : Widget(renderer, id, "select"), items_(items), style_(style) {
}

void Dropdown::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

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

    // Dropdown list (rendered when open)
    if (open_ && !items_.empty()) {
        float listY = y + h + 2;
        float listHeight = items_.size() * style_.itemHeight;

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
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    // Check main button
    if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
        open_ = !open_;
        return true;
    }

    // Check dropdown items when open
    if (open_ && !items_.empty()) {
        float listY = y + h + 2;
        float listHeight = items_.size() * style_.itemHeight;

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
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    float listY = y + h + 2;
    float listHeight = items_.size() * style_.itemHeight;

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
