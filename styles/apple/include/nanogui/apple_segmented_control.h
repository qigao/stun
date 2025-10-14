/*
    nanogui/apple_segmented_control.h -- Apple HIG segmented control

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleSegmentedControl : public Widget {
public:
  AppleSegmentedControl(Widget *parent, const std::vector<std::string> &items = {});

  int selected_index() const { return m_selected_index; }
  void set_selected_index(int index);

  const std::vector<std::string> &items() const { return m_items; }
  void set_items(const std::vector<std::string> &items);

  void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  int segment_at_position(const Vector2i &p) const;
  
  std::vector<std::string> m_items;
  int m_selected_index;
  int m_hover_index;
  std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
