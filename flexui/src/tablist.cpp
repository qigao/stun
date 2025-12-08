#include <flexui/tablist.h>
#include <cssbox_internal.h>

namespace flexui {

TabList::TabList(cssboxRenderer* renderer, const std::string& id,
                 const std::vector<std::string>& tabs, const TabListStyle& style)
    : Widget(renderer, id, "tablist"), tabs_(tabs), style_(style) {
}

void TabList::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    if (w == 0 || h == 0) return;

    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor defaultBgColor = cssBackground(style_.bgColor);
    NVGcolor defaultTextColor = cssColor(style_.textColor);

    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");

    float yPos = y;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        NVGcolor bgColor = defaultBgColor;
        NVGcolor textColor = defaultTextColor;

        if ((int)i == active_tab_) {
            bgColor = style_.activeBg;
            textColor = style_.activeTextColor;
        } else if ((int)i == hover_tab_) {
            bgColor = style_.hoverBg;
        }

        nvgBeginPath(vg);
        nvgRect(vg, x, yPos, style_.width, style_.tabHeight);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);

        nvgFillColor(vg, textColor);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + 16, yPos + style_.tabHeight / 2, tabs_[i].c_str(), nullptr);

        if ((int)i == active_tab_) {
            nvgBeginPath(vg);
            nvgRect(vg, x, yPos, 3, style_.tabHeight);
            nvgFillColor(vg, style_.activeTextColor);
            nvgFill(vg);
        }

        yPos += style_.tabHeight;
    }
}

bool TabList::handleMouseMove(float mx, float my) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;

    hover_tab_ = -1;
    float yPos = y;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (mx >= x && mx <= x + style_.width &&
            my >= yPos && my <= yPos + style_.tabHeight) {
            hover_tab_ = static_cast<int>(i);
            break;
        }
        yPos += style_.tabHeight;
    }
    return false;
}

bool TabList::handleMouseDown(float mx, float my) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;

    float yPos = y;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (mx >= x && mx <= x + style_.width &&
            my >= yPos && my <= yPos + style_.tabHeight) {
            active_tab_ = static_cast<int>(i);
            if (change_callback_) {
                change_callback_(active_tab_);
            }
            return true;
        }
        yPos += style_.tabHeight;
    }
    return false;
}

} // namespace flexui
