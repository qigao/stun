/*
    nanogui/apple_navigation_bar.h -- Apple HIG navigation bar

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <nanogui/apple_button.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleNavigationBar apple_navigation_bar.h nanogui/apple_navigation_bar.h
 *
 * \brief Apple HIG navigation bar (iOS/macOS)
 */
class NANOGUI_EXPORT AppleNavigationBar : public Widget {
public:
  enum class Style {
    Standard,  ///< Standard navigation bar
    Large      ///< Large title navigation bar (iOS 11+)
  };

  AppleNavigationBar(Widget *parent, const std::string &title = "",
                     Style style = Style::Standard);

  const std::string &title() const { return m_title; }
  void set_title(const std::string &title) { m_title = title; }

  Style style() const { return m_style; }
  void set_style(Style style) { m_style = style; }

  void set_left_button(const std::string &label, int icon,
                       const std::function<void()> &callback);
  void set_right_button(const std::string &label, int icon,
                        const std::function<void()> &callback);

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;

  std::string m_title;
  Style m_style;
  AppleButton *m_left_button;
  AppleButton *m_right_button;
};

NAMESPACE_END(nanogui)
