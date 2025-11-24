/*
    nanogui/apple_badge.h -- Apple HIG badge component

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT AppleBadge : public Widget {
public:
  AppleBadge(Widget *parent, const std::string &text = "");

  const std::string &text() const { return m_text; }
  void set_text(const std::string &text) { m_text = text; }

  int count() const { return m_count; }
  void set_count(int count) { m_count = count; }

  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  
  std::string m_text;
  int m_count;
};

NAMESPACE_END(nanogui)
