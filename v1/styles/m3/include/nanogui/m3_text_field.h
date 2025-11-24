/*
    nanogui/m3_text_field.h -- Material Design 3 Text Field

    Implements M3 text fields with filled and outlined variants.

    Based on: https://m3.material.io/components/text-fields

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/textbox.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3TextField m3_text_field.h nanogui/m3_text_field.h
 *
 * \brief Material Design 3 Text Field
 *
 * Text fields let users enter text. Two variants:
 * - Filled: Surface variant background with bottom indicator
 * - Outlined: Transparent background with outline border
 */
class NANOGUI_EXPORT M3TextField : public TextBox {
public:
    /// Text field style variants
    enum class Style {
        Filled,   ///< Filled background (default)
        Outlined  ///< Outlined border
    };

    /**
     * \brief Construct an M3 text field
     *
     * \param parent Parent widget
     * \param value Initial text value
     * \param style Text field style
     */
    M3TextField(Widget *parent, const std::string &value = "",
                Style style = Style::Filled);

    /// Set text field style
    void set_style(Style style) { m_style = style; }

    /// Get text field style
    Style style() const { return m_style; }

    /// Set label text
    void set_label(const std::string &label) { m_label = label; }

    /// Get label text
    const std::string &label() const { return m_label; }

    /// Set helper text
    void set_helper_text(const std::string &text) { m_helper_text = text; }

    /// Get helper text
    const std::string &helper_text() const { return m_helper_text; }

    /// Set error state
    void set_error(bool error) { m_error = error; }

    /// Get error state
    bool error() const { return m_error; }

    /// Draw the text field
    void draw(NVGcontext *ctx) override;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Calculate preferred size
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

private:
    void update_cursor(NVGcontext *ctx, float lastx, const NVGglyphPosition *glyphs, int size);
    float cursor_index_to_position(int index, float lastx, const NVGglyphPosition *glyphs, int size);
    int position_to_cursor_index(float posx, float lastx, const NVGglyphPosition *glyphs, int size);

protected:
    Style m_style;
    std::string m_label;
    std::string m_helper_text;
    bool m_error = false;
};

NAMESPACE_END(nanogui)
