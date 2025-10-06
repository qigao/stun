/*
    nanogui/apple_sheet.h -- Apple HIG sheet (modal presentation)

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/window.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleSheet apple_sheet.h nanogui/apple_sheet.h
 *
 * \brief Apple HIG sheet for modal presentation
 */
class NANOGUI_EXPORT AppleSheet : public Window {
public:
  enum class Style {
    Standard,    ///< Standard sheet
    FormSheet,   ///< Form sheet (smaller)
    FullScreen   ///< Full screen modal
  };

  AppleSheet(Widget *parent, const std::string &title = "",
             Style style = Style::Standard);

  Style style() const { return m_style; }
  void set_style(Style style) { m_style = style; }

  void show();
  void dismiss();

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;

  Style m_style;
  float m_animation_progress;
};

NAMESPACE_END(nanogui)
