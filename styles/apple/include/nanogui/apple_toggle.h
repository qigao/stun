/*
    nanogui/apple_toggle.h -- Apple HIG toggle/switch component

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleToggle : public Widget {
public:
  AppleToggle(Widget *parent, const std::string &caption = "");
  bool pushed() const { return m_pushed; }
  void set_pushed(bool pushed) { m_pushed = pushed; }
  void set_callback(const std::function<void(bool)> &callback) { m_callback = callback; }
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  bool m_pushed;
  std::string m_caption;
  std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
