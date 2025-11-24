/*
    nanogui/apple_progress.h -- Apple HIG progress indicator

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleProgress : public Widget {
public:
  enum class Style {
    Linear,    ///< Linear progress bar
    Circular   ///< Circular activity indicator
  };

  AppleProgress(Widget *parent, Style style = Style::Linear);

  float value() const { return m_value; }
  void set_value(float value) { m_value = std::max(0.0f, std::min(1.0f, value)); }

  Style style() const { return m_style; }
  void set_style(Style style) { m_style = style; }

  bool indeterminate() const { return m_indeterminate; }
  void set_indeterminate(bool indeterminate) { m_indeterminate = indeterminate; }

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  
  float m_value;
  Style m_style;
  bool m_indeterminate;
};

NAMESPACE_END(nanogui)
