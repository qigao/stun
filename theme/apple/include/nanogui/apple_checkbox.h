/*
    nanogui/apple_checkbox.h -- Apple HIG checkbox component

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleCheckbox : public Widget {
public:
  AppleCheckbox(Widget *parent, const std::string &caption = "",
                const std::function<void(bool)> &callback = nullptr);

  bool checked() const { return m_checked; }
  void set_checked(bool checked) { m_checked = checked; }
  
  const std::string &caption() const { return m_caption; }
  void set_caption(const std::string &caption) { m_caption = caption; }
  
  void set_callback(const std::function<void(bool)> &callback) { m_callback = callback; }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  
  bool m_checked;
  std::string m_caption;
  std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
