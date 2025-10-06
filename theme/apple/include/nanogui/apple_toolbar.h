/*
    nanogui/apple_toolbar.h -- Apple HIG toolbar

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <nanogui/apple_button.h>
#include <nanogui/layout.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleToolbar apple_toolbar.h nanogui/apple_toolbar.h
 *
 * \brief Apple HIG toolbar for action buttons
 */
class NANOGUI_EXPORT AppleToolbar : public Widget {
public:
  enum class Position {
    Top,
    Bottom
  };

  AppleToolbar(Widget *parent, Position position = Position::Top);

  Position position() const { return m_position; }
  void set_position(Position position) { m_position = position; }

  void add_button(const std::string &label, int icon,
                  const std::function<void()> &callback);
  void add_spacer();
  void add_flexible_space();

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;

  Position m_position;
};

NAMESPACE_END(nanogui)
