/*
    nanogui/apple_date_picker.h -- Apple HIG date/time picker

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleDatePicker apple_date_picker.h nanogui/apple_date_picker.h
 *
 * \brief Apple HIG date and time picker
 */
class NANOGUI_EXPORT AppleDatePicker : public Widget {
public:
  enum class Mode {
    Date,      ///< Date only
    Time,      ///< Time only
    DateTime   ///< Date and time
  };

  AppleDatePicker(Widget *parent, Mode mode = Mode::Date);

  Mode mode() const { return m_mode; }
  void set_mode(Mode mode) { m_mode = mode; }

  void set_date(int year, int month, int day);
  void set_time(int hour, int minute);

  int year() const { return m_year; }
  int month() const { return m_month; }
  int day() const { return m_day; }
  int hour() const { return m_hour; }
  int minute() const { return m_minute; }

  void set_callback(const std::function<void(int, int, int, int, int)> &callback) {
    m_callback = callback;
  }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;

  Mode m_mode;
  int m_year;
  int m_month;
  int m_day;
  int m_hour;
  int m_minute;
  int m_selected_component;
  std::function<void(int, int, int, int, int)> m_callback;
};

NAMESPACE_END(nanogui)
