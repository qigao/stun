/*
    nanogui/m3_tooltip_manager.h -- Material Design 3 Tooltip Manager

    Singleton manager for tooltip lifecycle and timing.
    Ensures only one tooltip is visible at a time.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/common.h>

NAMESPACE_BEGIN(nanogui)

// Forward declaration
class M3Tooltip;

/**
 * \class M3TooltipManager m3_tooltip_manager.h nanogui/m3_tooltip_manager.h
 *
 * \brief Singleton manager for M3 tooltip lifecycle
 *
 * Manages tooltip show/hide timing and ensures only one tooltip is visible at a time.
 * Implements requirement 10.7: "WHEN a tooltip is already visible and another is triggered 
 * THEN the system SHALL hide the first and show the second"
 */
class NANOGUI_EXPORT M3TooltipManager {
public:
    /// Get the singleton instance
    static M3TooltipManager &instance();

    /**
     * \brief Show a tooltip after a delay
     *
     * \param tooltip The tooltip to show
     * \param delay_ms Delay in milliseconds before showing (default 500ms)
     *
     * If another tooltip is currently visible or pending, it will be hidden first.
     * Persistent tooltips (like rich tooltips) will not auto-hide and require
     * explicit dismissal or action click.
     */
    void show_tooltip(M3Tooltip *tooltip, int delay_ms = 500);

    /**
     * \brief Hide a specific tooltip
     *
     * \param tooltip The tooltip to hide
     */
    void hide_tooltip(M3Tooltip *tooltip);

    /**
     * \brief Hide all tooltips immediately
     */
    void hide_all();

    /**
     * \brief Update tooltip timing (called each frame)
     *
     * \param dt Delta time in seconds
     *
     * Tracks the show timer and displays the tooltip when the delay expires.
     */
    void update(float dt);

    /// Get the currently visible or pending tooltip
    M3Tooltip *current_tooltip() const { return m_current_tooltip; }

private:
    /// Private constructor for singleton pattern
    M3TooltipManager() = default;

    /// Prevent copying
    M3TooltipManager(const M3TooltipManager&) = delete;
    M3TooltipManager &operator=(const M3TooltipManager&) = delete;

    M3Tooltip *m_current_tooltip = nullptr;
    float m_show_timer = 0.0f;
    int m_show_delay = 500;
    bool m_pending_show = false;
};

NAMESPACE_END(nanogui)
