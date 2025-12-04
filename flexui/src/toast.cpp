#include <flexui/toast.h>
#include <cssbox_internal.h>
namespace flexui {

Toast::Toast(cssboxRenderer *renderer, const std::string &id, const std::string &message,
             ToastType type, const ToastStyle &style)
    : Widget(renderer, id, "toast"), message_(message), type_(type), style_(style) {
  updateStyleForType();
}

void Toast::updateStyleForType() {
  // Colors are already in style_, no need to update
}

void Toast::setType(ToastType type) {
  type_ = type;
  updateStyleForType();
}

void Toast::show(float duration) {
  visible_ = true;
  duration_ = duration;
  show_time_ = std::chrono::steady_clock::now();
}

void Toast::hide() {
  if (visible_) {
    visible_ = false;
    if (dismiss_callback_) {
      dismiss_callback_();
    }
  }
}

void Toast::updateTimer() {
  if (!visible_)
    return;

  auto now = std::chrono::steady_clock::now();
  float elapsed = std::chrono::duration<float>(now - show_time_).count();

  if (elapsed >= duration_) {
    hide();
  }
}

void Toast::draw(NVGcontext *vg) {
  if (!visible_)
    return;

  auto *el = element();
  float x = el->computed.x;
  float y = el->computed.y;
  float w = el->computed.width;
  float h = el->computed.height;

  if (w == 0 || h == 0)
    return;

  // Update timer
  updateTimer();

  // Background color based on type
  NVGcolor typeBg;
  switch (type_) {
  case ToastType::Info:
    typeBg = style_.infoColor;
    break;
  case ToastType::Success:
    typeBg = style_.successColor;
    break;
  case ToastType::Warning:
    typeBg = style_.warningColor;
    break;
  case ToastType::Error:
    typeBg = style_.errorColor;
    break;
  }

  NVGcolor bgColor = cssBackground(typeBg);
  float borderRadius = cssBorderRadius(style_.borderRadius);
  float fontSize = cssFontSize(style_.fontSize);
  NVGcolor textColor = cssColor(style_.textColor);

  // Draw background
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, borderRadius);
  nvgFillColor(vg, bgColor);
  nvgFill(vg);

  // Draw message
  nvgFontSize(vg, fontSize);
  nvgFontFace(vg, "sans-serif");
  nvgFillColor(vg, textColor);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, x + w / 2, y + h / 2, message_.c_str(), nullptr);
}

} // namespace flexui
