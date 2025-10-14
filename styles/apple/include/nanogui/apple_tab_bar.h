/*
    nanogui/apple_tab_bar.h -- Apple HIG tab bar (iOS)

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleTabBar apple_tab_bar.h nanogui/apple_tab_bar.h
 *
 * \brief Apple HIG tab bar for bottom navigation (iOS)
 */
class NANOGUI_EXPORT AppleTabBar : public Widget {
public:
  struct Tab {
    std::string label;
    int icon;
    std::function<void()> callback;

    Tab(const std::string &label = "", int icon = 0,
        const std::function<void()> &callback = nullptr)
        : label(label), icon(icon), callback(callback) {}
  };

  AppleTabBar(Widget *parent);

  void add_tab(const std::string &label, int icon,
               const std::function<void()> &callback = nullptr);

  int selected_index() const { return m_selected_index; }
  void set_selected_index(int index);

  const std::vector<Tab> &tabs() const { return m_tabs; }

  bool mouse_button_event(const Vector2i &p, int button, bool down,
                          int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  int tab_at_position(const Vector2i &p) const;

  std::vector<Tab> m_tabs;
  int m_selected_index;
  int m_hover_index;
};

NAMESPACE_END(nanogui)
