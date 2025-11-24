/*
    nanogui/apple_popover.h -- Apple HIG popover

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class ApplePopover apple_popover.h nanogui/apple_popover.h
 *
 * \brief Apple HIG popover for contextual content
 */
class NANOGUI_EXPORT ApplePopover : public Popup {
public:
  enum class ArrowPosition {
    Top,
    Bottom,
    Left,
    Right,
    None
  };

  ApplePopover(Widget *parent, Widget *anchor = nullptr);

  ArrowPosition arrow_position() const { return m_arrow_position; }
  void set_arrow_position(ArrowPosition pos) { m_arrow_position = pos; }

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  void draw_arrow(NVGcontext *ctx, float x, float y);

  ArrowPosition m_arrow_position;
};

NAMESPACE_END(nanogui)
