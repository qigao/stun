/*
    nanogui/fluent_chip.h -- Fluent Design Chip widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentChip fluent_chip.h nanogui/fluent_chip.h
 *
 * \brief Fluent Design Chip widget.
 *
 * Chips are compact elements that represent an input, attribute, or action.
 * Variants: Input, Filter, Action, Suggestion
 */
class NANOGUI_EXPORT FluentChip : public Widget {
public:
  enum class Variant {
    Input,     // Represents user input (with optional close)
    Filter,    // Toggleable filter option (with checkmark)
    Action,    // Triggers an action
    Suggestion // Suggestion for user
  };

  FluentChip(Widget *parent, const std::string &label,
               Variant variant = Variant::Action);

  const std::string &label() const { return m_label; }
  void set_label(const std::string &label) { m_label = label; }

  Variant variant() const { return m_variant; }
  void set_variant(Variant variant) { m_variant = variant; }

  int icon() const { return m_icon; }
  void set_icon(int icon) { m_icon = icon; }

  bool selected() const { return m_selected; }
  void set_selected(bool selected) { m_selected = selected; }

  bool closeable() const { return m_closeable; }
  void set_closeable(bool closeable) { m_closeable = closeable; }

  const std::function<void()> &callback() const { return m_callback; }
  void set_callback(const std::function<void()> &callback) {
    m_callback = callback;
  }

  const std::function<void()> &close_callback() const {
    return m_close_callback;
  }
  void set_close_callback(const std::function<void()> &callback) {
    m_close_callback = callback;
  }

  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down,
                          int modifiers) override;
  void draw(NVGcontext *ctx) override;

protected:
  std::string m_label;
  Variant m_variant;
  int m_icon;
  bool m_selected;
  bool m_closeable;
  std::function<void()> m_callback;
  std::function<void()> m_close_callback;
  bool m_mouse_over_close;
};

NAMESPACE_END(nanogui)
