/*
    nanogui/apple_button.h -- Apple HIG button component

    Implements Apple-style buttons with proper system colors and styling.

    Based on: https://developer.apple.com/design/human-interface-guidelines/buttons

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleButton apple_button.h nanogui/apple_button.h
 *
 * \brief Apple HIG button component
 *
 * Implements Apple button styles:
 * - Primary: Filled with accent color (high emphasis)
 * - Secondary: Bordered with subtle fill (medium emphasis)
 * - Tertiary: Text-only, minimal style (low emphasis)
 * - Destructive: Red accent for dangerous actions
 */
class NANOGUI_EXPORT AppleButton : public Button {
public:
  /// Apple button style variants
  enum class Style {
    Primary,     ///< Filled with accent color
    Secondary,   ///< Bordered with subtle background
    Tertiary,    ///< Text-only, minimal
    Destructive  ///< Red accent for dangerous actions
  };

  /**
   * \brief Construct an Apple button
   *
   * \param parent Parent widget
   * \param caption Button text
   * \param icon Optional icon (Font Awesome code)
   * \param style Button style variant
   */
  AppleButton(Widget *parent, const std::string &caption = "Button",
              int icon = 0, Style style = Style::Primary);

  /// Set button style
  void set_style(Style style);

  /// Get button style
  Style style() const { return m_style; }

  /// Draw the button
  void draw(NVGcontext *ctx) override;

protected:
  /// Calculate preferred size
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

  /// Get Apple theme (cast from base theme)
  AppleTheme *apple_theme() const;

  /// Get background color for current style and state
  Color get_background_color() const;

  /// Get text color for current style and state
  Color get_text_color() const;

  /// Get border color for secondary style
  Color get_border_color() const;

  Style m_style;
};

NAMESPACE_END(nanogui)
