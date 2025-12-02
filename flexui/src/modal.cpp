#include <flexui/modal.h>
#include <nanovg_css_internal.h>

namespace flexui {

Modal::Modal(NVGCSSRenderer* renderer, const std::string& id,
             const std::string& title, const std::string& content,
             const ModalStyle& style)
    : Widget(renderer, id, "modal"), title_(title), content_(content), style_(style) {
}

void Modal::draw(NVGcontext* vg) {
    if (!visible_) return;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);
    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    // Draw modal background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Draw title bar
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, style_.titleHeight);
    nvgFillColor(vg, style_.titleBg);
    nvgFill(vg);

    // Draw title text
    nvgFontSize(vg, fontSize + 2);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + 16, y + style_.titleHeight / 2, title_.c_str(), nullptr);

    // Draw close button (X)
    float closeX = x + w - 30;
    float closeY = y + style_.titleHeight / 2;
    nvgFontSize(vg, 18);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, closeX, closeY, "×", nullptr);

    // Draw content
    nvgFontSize(vg, fontSize);
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(vg, x + 16, y + style_.titleHeight + 16,
               w - 32, content_.c_str(), nullptr);
}

bool Modal::handleMouseDown(float mx, float my) {
    if (!visible_) return false;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    // Check close button click
    float closeX = x + w - 30;
    float closeY = y + style_.titleHeight / 2;
    if (mx >= closeX - 15 && mx <= closeX + 15 &&
        my >= closeY - 15 && my <= closeY + 15) {
        hide();
        if (close_callback_) {
            close_callback_();
        }
        return true;
    }

    // Check if clicked outside modal
    if (mx < x || mx > x + w ||
        my < y || my > y + h) {
        hide();
        if (close_callback_) {
            close_callback_();
        }
        return true;
    }

    return true; // Consume click if inside modal
}

} // namespace flexui
