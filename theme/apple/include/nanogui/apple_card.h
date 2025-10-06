/*
    nanogui/apple_card.h -- Apple HIG card component

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <nanogui/layout.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleCard : public Widget {
public:
  AppleCard(Widget *parent, const std::string &title = "");

  const std::string &title() const { return m_title; }
  void set_title(const std::string &title) { m_title = title; }

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  
  std::string m_title;
};

NAMESPACE_END(nanogui)
