/*
    nanogui/apple_alert.h -- Apple HIG alert dialog

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/window.h>
#include <nanogui/apple_theme.h>
#include <nanogui/apple_button.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleAlert apple_alert.h nanogui/apple_alert.h
 *
 * \brief Apple HIG alert dialog
 */
class NANOGUI_EXPORT AppleAlert : public Window {
public:
  enum class Style {
    Alert,        ///< Standard alert
    ActionSheet   ///< Action sheet (iOS)
  };

  AppleAlert(Widget *parent, const std::string &title = "",
             const std::string &message = "", Style style = Style::Alert);

  const std::string &message() const { return m_message; }
  void set_message(const std::string &message);

  void add_button(const std::string &label, const std::function<void()> &callback,
                  AppleButton::Style style = AppleButton::Style::Primary);

  void show();
  void dismiss();

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;

  Style m_style;
  std::string m_message;
  Widget *m_button_container;
  Widget *m_content_container;
};

NAMESPACE_END(nanogui)
