/*
    nanogui/apple_slider.h -- Apple HIG slider component

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleSlider : public Widget {
public:
  AppleSlider(Widget *parent);

  float value() const { return m_value; }
  void set_value(float value) { m_value = std::max(0.0f, std::min(1.0f, value)); }

  void set_callback(const std::function<void(float)> &callback) { m_callback = callback; }
  void set_final_callback(const std::function<void(float)> &callback) { m_final_callback = callback; }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  
  float m_value;
  std::function<void(float)> m_callback;
  std::function<void(float)> m_final_callback;
  bool m_dragging;
};

NAMESPACE_END(nanogui)
