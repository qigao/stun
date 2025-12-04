#include <flexui/button.h>
#include <cssbox_internal.h>

namespace flexui {

Button::Button(cssboxRenderer *renderer, const std::string &id, const std::string &text,
               const ButtonStyle &style)
    : Widget(renderer, id, "button"), text_(text), style_(style) {}

void Button::draw(NVGcontext *vg) {
  auto *el = element();
  float x = el->computed.x;
  float y = el->computed.y;
  float w = el->computed.width;
  float h = el->computed.height;

  // Determine background color with state-aware fallback
  bool hovered = el->pseudo_states.count("hover") > 0;
  NVGcolor fallbackBg = pressed_ ? style_.bgColorPressed : (hovered ? style_.bgColorHover : style_.bgColor);
  NVGcolor bgColor = cssBackground(fallbackBg);
  float borderRadius = cssBorderRadius(style_.borderRadius);
  float fontSize = cssFontSize(style_.fontSize);
  NVGcolor textColor = el->style.color;

  // Button background
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, borderRadius);
  nvgFillColor(vg, bgColor);
  nvgFill(vg);

  // Button text
  nvgFontSize(vg, fontSize);
  nvgFontFace(vg, "sans-serif");
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, textColor);
  nvgText(vg, x + w / 2, y + h / 2, text_.c_str(), nullptr);
}

} // namespace flexui
