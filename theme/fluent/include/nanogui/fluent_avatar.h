/*
    nanogui/fluent_avatar.h -- Fluent Design Avatar widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentAvatar fluent_avatar.h nanogui/fluent_avatar.h
 *
 * \brief Fluent Design Avatar widget.
 *
 * Avatars display user profile images or initials in a circular container.
 * Supports images, icons, or text initials.
 */
class NANOGUI_EXPORT FluentAvatar : public Widget {
public:
    enum class Size {
        Small,    // 24px
        Medium,   // 40px
        Large     // 56px
    };

    FluentAvatar(Widget *parent, const std::string &initials = "", Size size = Size::Medium);

    const std::string &initials() const { return m_initials; }
    void set_initials(const std::string &initials) { m_initials = initials; }

    int icon() const { return m_icon; }
    void set_icon(int icon) { m_icon = icon; }

    Size avatar_size() const { return m_avatar_size; }
    void set_avatar_size(Size size);

    const Color &background_color() const { return m_background_color; }
    void set_background_color(const Color &color) { m_background_color = color; }

    const Color &text_color() const { return m_text_color; }
    void set_text_color(const Color &color) { m_text_color = color; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    std::string m_initials;
    int m_icon;
    Size m_avatar_size;
    Color m_background_color;
    Color m_text_color;
};

NAMESPACE_END(nanogui)
