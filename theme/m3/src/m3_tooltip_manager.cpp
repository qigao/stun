/*
    src/m3_tooltip_manager.cpp -- Material Design 3 Tooltip Manager implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_tooltip_manager.h>
#include <nanogui/m3_tooltip.h>
#include <iostream>

NAMESPACE_BEGIN(nanogui)

M3TooltipManager &M3TooltipManager::instance() {
    static M3TooltipManager instance;
    return instance;
}

void M3TooltipManager::show_tooltip(M3Tooltip *tooltip, int delay_ms) {
    if (!tooltip)
        return;

    std::cout << "M3TooltipManager::show_tooltip called with delay=" << delay_ms << std::endl;

    // If there's already a tooltip visible or pending, hide it first
    // This implements requirement 10.7
    if (m_current_tooltip && m_current_tooltip != tooltip) {
        m_current_tooltip->hide_immediate(false);
    }

    // Set up the new tooltip
    m_current_tooltip = tooltip;
    m_show_delay = delay_ms;
    m_show_timer = 0.0f;
    m_pending_show = true;
}

void M3TooltipManager::hide_tooltip(M3Tooltip *tooltip) {
    if (!tooltip)
        return;

    std::cout << "M3TooltipManager::hide_tooltip called" << std::endl;

    // Only hide if this is the current tooltip
    if (m_current_tooltip == tooltip) {
        tooltip->hide_immediate(false);
        m_current_tooltip = nullptr;
        m_pending_show = false;
        m_show_timer = 0.0f;
    }
}

void M3TooltipManager::hide_all() {
    if (m_current_tooltip) {
        m_current_tooltip->hide_immediate(false);
        m_current_tooltip = nullptr;
    }
    m_pending_show = false;
    m_show_timer = 0.0f;
}

void M3TooltipManager::update(float dt) {
    // Handle pending show timer
    if (m_pending_show && m_current_tooltip) {
        m_show_timer += dt;
        std::cout << "TooltipManager update: timer=" << m_show_timer << " delay=" << (m_show_delay / 1000.0f) << std::endl;

        // Convert delay from milliseconds to seconds
        float delay_seconds = m_show_delay / 1000.0f;

        if (m_show_timer >= delay_seconds) {
            // Timer expired, show the tooltip
            std::cout << "TooltipManager: timer expired, showing tooltip" << std::endl;
            m_pending_show = false;
            m_show_timer = 0.0f;
            m_current_tooltip->show_immediate();
        }
    }
}

NAMESPACE_END(nanogui)
