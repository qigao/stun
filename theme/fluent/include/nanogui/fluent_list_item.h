/*
    nanogui/fluent_list_item.h -- Fluent Design List Item widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentListItem fluent_list_item.h nanogui/fluent_list_item.h
 *
 * \brief Fluent Design List Item widget.
 *
 * A single item in a Fluent Design list.
 * Can contain text, icons, and trailing elements.
 */
class NANOGUI_EXPORT FluentListItem : public Widget {
public:
    enum class Type {
        OneLine,    // Single line of text
        TwoLine,    // Primary and secondary text
        ThreeLine   // Primary, secondary, and tertiary text
    };

    FluentListItem(Widget *parent, const std::string &primary_text, Type type = Type::OneLine);

    const std::string &primary_text() const { return m_primary_text; }
    void set_primary_text(const std::string &text) { m_primary_text = text; }

    const std::string &secondary_text() const { return m_secondary_text; }
    void set_secondary_text(const std::string &text) { m_secondary_text = text; }

    int leading_icon() const { return m_leading_icon; }
    void set_leading_icon(int icon) { m_leading_icon = icon; }

    int trailing_icon() const { return m_trailing_icon; }
    void set_trailing_icon(int icon) { m_trailing_icon = icon; }

    const std::function<void()> &callback() const { return m_callback; }
    void set_callback(const std::function<void()> &callback) { m_callback = callback; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    bool mouse_enter_event(const Vector2i &p, bool enter) override;

protected:
    std::string m_primary_text;
    std::string m_secondary_text;
    int m_leading_icon;
    int m_trailing_icon;
    Type m_type;
    std::function<void()> m_callback;
    bool m_mouse_over;
};

NAMESPACE_END(nanogui)
