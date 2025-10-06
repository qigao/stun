/*
    nanogui/fluent_text_field.h -- Fluent Design Text Field widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/textbox.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentTextField fluent_text_field.h nanogui/fluent_text_field.h
 *
 * \brief Fluent Design Text Field widget.
 *
 * Enhanced text input with Fluent Design styling:
 * - Floating label
 * - Helper text
 * - Error states
 * - Leading/trailing icons
 * - Variants: Filled, Outlined
 */
class NANOGUI_EXPORT FluentTextField : public TextBox {
public:
    enum class Variant {
        Filled,    // Filled background
        Outlined   // Outlined border
    };

    FluentTextField(Widget *parent, const std::string &value = "", 
                     Variant variant = Variant::Filled);

    const std::string &label() const { return m_label; }
    void set_label(const std::string &label) { m_label = label; }

    const std::string &helper_text() const { return m_helper_text; }
    void set_helper_text(const std::string &text) { m_helper_text = text; }

    bool error() const { return m_error; }
    void set_error(bool error) { m_error = error; }

    const std::string &error_text() const { return m_error_text; }
    void set_error_text(const std::string &text) { m_error_text = text; }

    int leading_icon() const { return m_leading_icon; }
    void set_leading_icon(int icon) { m_leading_icon = icon; }

    int trailing_icon() const { return m_trailing_icon; }
    void set_trailing_icon(int icon) { m_trailing_icon = icon; }

    Variant variant() const { return m_variant; }
    void set_variant(Variant variant) { m_variant = variant; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool focus_event(bool focused) override;

protected:
    std::string m_label;
    std::string m_helper_text;
    std::string m_error_text;
    bool m_error;
    int m_leading_icon;
    int m_trailing_icon;
    Variant m_variant;
    bool m_label_floating;
};

NAMESPACE_END(nanogui)
