/*
    nanogui/m3_chip.h -- Material Design 3 Chip component

    Implements M3 chips with multiple variants.

    Based on: https://m3.material.io/components/chips

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Chip m3_chip.h nanogui/m3_chip.h
 *
 * \brief Material Design 3 Chip component
 *
 * Chips help users enter information, select choices, filter content, or trigger actions.
 * Four variants:
 * - Assist: Help users take action
 * - Filter: Refine content
 * - Input: Represent discrete information
 * - Suggestion: Offer dynamic recommendations
 */
class NANOGUI_EXPORT M3Chip : public Button {
public:
    /// Chip style variants
    enum class Style {
        Assist,     ///< Action chip
        Filter,     ///< Filter chip (can be selected)
        Input,      ///< Input chip (can be removed)
        Suggestion  ///< Suggestion chip
    };

    /**
     * \brief Construct an M3 chip
     *
     * \param parent Parent widget
     * \param label Chip text
     * \param icon Optional icon
     * \param style Chip style
     */
    M3Chip(Widget *parent, const std::string &label,
           int icon = 0, Style style = Style::Assist);

    /// Set chip style
    void set_style(Style style) { m_style = style; }

    /// Get chip style
    Style style() const { return m_style; }

    /// Set selected state (for Filter chips)
    void set_selected(bool selected) { m_selected = selected; }

    /// Get selected state
    bool selected() const { return m_selected; }

    /// Set removable (for Input chips)
    void set_removable(bool removable) { m_removable = removable; }

    /// Get removable state
    bool removable() const { return m_removable; }

    /// Draw the chip
    void draw(NVGcontext *ctx) override;

protected:
    /// Calculate preferred size
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Draw state layer
    void draw_state_layer(NVGcontext *ctx, float x, float y, float w, float h);

    Style m_style;
    bool m_selected = false;
    bool m_removable = false;
};

NAMESPACE_END(nanogui)
