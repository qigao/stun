/*
    nanogui/fluent_tabs.h -- Fluent Design Tabs widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/tabwidget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentTabs fluent_tabs.h nanogui/fluent_tabs.h
 *
 * \brief Fluent Design Tabs widget.
 *
 * Enhanced TabWidget with Fluent Design styling including
 * animated indicator and proper spacing.
 */
class NANOGUI_EXPORT FluentTabs : public TabWidget {
public:
    FluentTabs(Widget *parent);

    void draw(NVGcontext *ctx) override;

protected:
    float m_indicator_position = 0.f;
    float m_indicator_width = 0.f;
};

NAMESPACE_END(nanogui)
