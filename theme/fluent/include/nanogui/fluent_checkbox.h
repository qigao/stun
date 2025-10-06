/*
    nanogui/fluent_checkbox.h -- Fluent Design Checkbox widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentCheckbox fluent_checkbox.h nanogui/fluent_checkbox.h
 *
 * \brief Fluent Design Checkbox widget.
 *
 * Checkbox with Fluent Design styling including ripple effect area
 * and proper state colors.
 */
class NANOGUI_EXPORT FluentCheckbox : public Widget {
public:
    FluentCheckbox(Widget *parent, const std::string &caption = "");

    bool checked() const { return m_checked; }
    void set_checked(bool checked) { m_checked = checked; }

    const std::string &caption() const { return m_caption; }
    void set_caption(const std::string &caption) { m_caption = caption; }

    const std::function<void(bool)> &callback() const { return m_callback; }
    void set_callback(const std::function<void(bool)> &callback) { m_callback = callback; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;

protected:
    bool m_checked;
    std::string m_caption;
    std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
