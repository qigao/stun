/*
    nanogui/fluent_dialog.h -- Fluent Design Dialog widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/window.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentDialog fluent_dialog.h nanogui/fluent_dialog.h
 *
 * \brief Fluent Design Dialog widget.
 *
 * A modal dialog with Fluent Design styling.
 * Contains title, content area, and action buttons.
 */
class NANOGUI_EXPORT FluentDialog : public Window {
public:
    FluentDialog(Widget *parent, const std::string &title = "");

    /// Get the content widget (add your content here)
    Widget *content() { return m_content; }

    /// Get the actions widget (add buttons here)
    Widget *actions() { return m_actions; }

    /// Add an action button
    void add_action(const std::string &label, const std::function<void()> &callback);

    void draw(NVGcontext *ctx) override;

protected:
    Widget *m_content;
    Widget *m_actions;
};

NAMESPACE_END(nanogui)
