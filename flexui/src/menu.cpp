#include <flexui/menu.h>
#include <cssbox_internal.h>
#include <algorithm>

namespace flexui {

Menu::Menu(cssboxRenderer* renderer, const std::string& id,
           const std::vector<MenuItem>& items, const MenuStyle& style)
    : Widget(renderer, id, "menu"), items_(items), style_(style) {
}

void Menu::addItem(const MenuItem& item) {
    items_.push_back(item);
}

void Menu::show() {
    visible_ = true;
}

void Menu::hide() {
    visible_ = false;
    hovered_index_ = -1;
}

void Menu::draw(NVGcontext* vg) {
    if (!visible_) return;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, style_.borderRadius);
    nvgFillColor(vg, style_.backgroundColor);
    nvgFill(vg);

    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    for (size_t i = 0; i < items_.size(); ++i) {
        float itemY = y + static_cast<float>(i) * style_.itemHeight;

        // Highlight hovered item
        if (hovered_index_ == static_cast<int>(i) && items_[i].enabled) {
            nvgBeginPath(vg);
            nvgRect(vg, x, itemY, w, style_.itemHeight);
            nvgFillColor(vg, style_.itemHoverColor);
            nvgFill(vg);
        }

        // Text
        if (items_[i].enabled) {
            nvgFillColor(vg, style_.textColor);
        } else {
            nvgFillColor(vg, style_.disabledTextColor);
        }
        nvgText(vg, x + style_.padding, itemY + style_.itemHeight / 2, items_[i].text.c_str(), nullptr);
    }
}

bool Menu::handleMouseMove(float mx, float my) {
    if (!visible_) return false;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;

    hovered_index_ = -1;
    for (size_t i = 0; i < items_.size(); ++i) {
        float itemY = y + static_cast<float>(i) * style_.itemHeight;
        if (mx >= x && mx <= x + w &&
            my >= itemY && my <= itemY + style_.itemHeight) {
            hovered_index_ = static_cast<int>(i);
            break;
        }
    }
    return false;
}

bool Menu::handleMouseDown(float mx, float my) {
    if (!visible_) return false;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
        if (hovered_index_ != -1 && hovered_index_ < static_cast<int>(items_.size()) &&
            items_[hovered_index_].enabled) {
            if (items_[hovered_index_].callback) {
                items_[hovered_index_].callback();
            }
            hide();
            return true;
        }
    }
    return false;
}

} // namespace flexui
