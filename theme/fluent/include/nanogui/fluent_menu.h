/*
    nanogui/fluent_menu.h -- Fluent Design Menu widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/popup.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentMenu fluent_menu.h nanogui/fluent_menu.h
 *
 * \brief Fluent Design Menu widget.
 *
 * A popup menu with Fluent Design styling.
 * Contains menu items that can be clicked.
 */
class NANOGUI_EXPORT FluentMenu : public Popup {
public:
    FluentMenu(Widget *parent, Widget *anchor = nullptr);

    /// Add a menu item
    void add_item(const std::string &label, const std::function<void()> &callback, int icon = 0);

    /// Add a divider
    void add_divider();

    void draw(NVGcontext *ctx) override;

protected:
    class MenuItem;
};

NAMESPACE_END(nanogui)
