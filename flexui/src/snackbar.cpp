#include <flexui/snackbar.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

Snackbar::Snackbar(NVGCSSRenderer* renderer, const std::string& id,
                   const std::string& message, const SnackbarStyle& style)
    : Widget(renderer, id, "snackbar"), message_(message), style_(style) {
}

void Snackbar::show(float duration) {
    visible_ = true;
    duration_ = duration;
    elapsed_ = 0.0f;
}

void Snackbar::draw(NVGcontext* vg) {
    if (!visible_) return;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    NVGcolor bgColor = cssBackground(style_.backgroundColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);
    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");

    // Message
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + style_.padding, y + h / 2, message_.c_str(), nullptr);

    // Action button
    if (!action_text_.empty()) {
        nvgFillColor(vg, style_.actionColor);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + w - style_.padding, y + h / 2, action_text_.c_str(), nullptr);
    }
}

bool Snackbar::handleMouseDown(float mx, float my) {
    if (!visible_ || action_text_.empty()) return false;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
        if (action_callback_) {
            action_callback_();
        }
        hide();
        return true;
    }
    return false;
}

void Snackbar::update(float dt) {
    if (!visible_) return;

    elapsed_ += dt;
    if (elapsed_ >= duration_) {
        hide();
    }
}

} // namespace flexui
