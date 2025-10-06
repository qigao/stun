/*
    nanogui/fluent_snackbar.h -- Fluent Design Snackbar widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentSnackbar fluent_snackbar.h nanogui/fluent_snackbar.h
 *
 * \brief Fluent Design Snackbar widget.
 *
 * Snackbars provide brief messages about app processes at the bottom of the screen.
 * They can contain an optional action button and auto-dismiss after a timeout.
 */
class NANOGUI_EXPORT FluentSnackbar : public Widget {
public:
    FluentSnackbar(Widget *parent, const std::string &message);

    const std::string &message() const { return m_message; }
    void set_message(const std::string &message) { m_message = message; }

    const std::string &action_text() const { return m_action_text; }
    void set_action_text(const std::string &text) { m_action_text = text; }

    const std::function<void()> &action_callback() const { return m_action_callback; }
    void set_action_callback(const std::function<void()> &callback) { m_action_callback = callback; }

    int duration_ms() const { return m_duration_ms; }
    void set_duration_ms(int ms) { m_duration_ms = ms; }

    /// Show the snackbar with animation
    void show();

    /// Hide the snackbar with animation
    void hide();

    /// Check if snackbar is currently visible
    bool is_showing() const { return m_showing; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

protected:
    std::string m_message;
    std::string m_action_text;
    std::function<void()> m_action_callback;
    int m_duration_ms;
    bool m_showing;
    std::chrono::steady_clock::time_point m_show_time;
    float m_animation_progress; // 0.0 to 1.0
};

NAMESPACE_END(nanogui)
