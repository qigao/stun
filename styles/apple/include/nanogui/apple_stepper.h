/*
    nanogui/apple_stepper.h -- Apple HIG stepper control

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleStepper apple_stepper.h nanogui/apple_stepper.h
 *
 * \brief Apple HIG stepper for incrementing/decrementing values
 */
class NANOGUI_EXPORT AppleStepper : public Widget {
public:
  AppleStepper(Widget *parent);

  int value() const { return m_value; }
  void set_value(int value);

  int min_value() const { return m_min_value; }
  void set_min_value(int min) { m_min_value = min; }

  int max_value() const { return m_max_value; }
  void set_max_value(int max) { m_max_value = max; }

  int step() const { return m_step; }
  void set_step(int step) { m_step = step; }

  void set_callback(const std::function<void(int)> &callback) {
    m_callback = callback;
  }

  void increment();
  void decrement();

  bool mouse_button_event(const Vector2i &p, int button, bool down,
                          int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;

  int m_value;
  int m_min_value;
  int m_max_value;
  int m_step;
  bool m_minus_pressed;
  bool m_plus_pressed;
  std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
