/*
    nanogui/m3_snackbar.h -- Material Design 3 Snackbar

    Implements M3 snackbar for brief messages.

    Based on: https://m3.material.io/components/snackbar

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Snackbar m3_snackbar.h nanogui/m3_snackbar.h
 *
 * \brief Material Design 3 Snackbar
 *
 * Snackbars provide brief messages about app processes at the bottom of the screen.
 */
class NANOGUI_EXPORT M3Snackbar : public Widget {
public:
    /**
     * \brief Construct an M3 snackbar
     *
     * \param parent Parent widget (usually Screen)
     * \param message Snackbar message
     * \param action_label Optional action button label
     */
    M3Snackbar(Widget *parent, const std::string &message = "",
               const std::string &action_label = "");

    /// Set message
    void set_message(const std::string &message) { m_message = message; }

    /// Get message
    const std::string &message() const { return m_message; }

    /// Set action label
    void set_action_label(const std::string &label) { m_action_label = label; }

    /// Get action label
    const std::string &action_label() const { return m_action_label; }

    /// Set action callback
    void set_action_callback(const std::function<void()> &callback) { m_action_callback = callback; }

    /// Get action callback
    const std::function<void()> &action_callback() const { return m_action_callback; }

    /// Show snackbar with auto-dismiss
    void show(float duration = 4.0f);

    /// Dismiss snackbar
    void dismiss();

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Draw the snackbar
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Check if action button is at position
    bool action_at_position(const Vector2i &p) const;

    std::string m_message;
    std::string m_action_label;
    std::function<void()> m_action_callback;
    float m_show_time = 0.0f;
    float m_duration = 4.0f;
    bool m_action_hover = false;
};

NAMESPACE_END(nanogui)
