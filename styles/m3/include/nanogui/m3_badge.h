/*
    nanogui/m3_badge.h -- Material Design 3 Badge

    Implements M3 badge for notifications.

    Based on: https://m3.material.io/components/badges

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Badge m3_badge.h nanogui/m3_badge.h
 *
 * \brief Material Design 3 Badge
 *
 * Badges show notifications, counts, or status information.
 */
class NANOGUI_EXPORT M3Badge : public Widget {
public:
    /**
     * \brief Construct an M3 badge
     *
     * \param parent Parent widget
     * \param count Badge count (0 for dot badge)
     */
    M3Badge(Widget *parent, int count = 0);

    /// Set badge count
    void set_count(int count) { m_count = count; }

    /// Get badge count
    int count() const { return m_count; }

    /// Draw the badge
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    int m_count;
};

NAMESPACE_END(nanogui)
