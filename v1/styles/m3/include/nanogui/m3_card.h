/*
    nanogui/m3_card.h -- Material Design 3 Card component

    Implements M3 cards with elevation and proper surface colors.

    Based on: https://m3.material.io/components/cards

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/m3_theme.h>
#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Card m3_card.h nanogui/m3_card.h
 *
 * \brief Material Design 3 Card component
 *
 * Cards contain content and actions about a single subject.
 * Three variants:
 * - Elevated: Surface with elevation tint and shadow
 * - Filled: Surface variant color
 * - Outlined: Surface with outline border
 */
class NANOGUI_EXPORT M3Card : public Widget {
public:
  /// Card style variants
  enum class Style {
    Elevated, ///< Surface with elevation (default)
    Filled,   ///< Surface variant color
    Outlined  ///< Surface with outline
  };

  /**
   * \brief Construct an M3 card
   *
   * \param parent Parent widget
   * \param style Card style
   */
  M3Card(Widget *parent, Style style = Style::Elevated);

  /// Set card style
  void set_style(Style style) { m_style = style; }

  /// Get card style
  Style style() const { return m_style; }

  /// Set elevation level (for Elevated style)
  void set_elevation(M3Theme::Elevation level) { m_elevation = level; }

  /// Get elevation level
  M3Theme::Elevation elevation() const { return m_elevation; }

  /// Draw the card
  void draw(NVGcontext *ctx) override;

protected:
  /// Get M3 theme
  M3Theme *m3_theme() const;

  Style m_style;
  M3Theme::Elevation m_elevation = M3Theme::Elevation::Level1;
};

NAMESPACE_END(nanogui)
