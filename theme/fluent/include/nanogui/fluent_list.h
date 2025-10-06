/*
    nanogui/fluent_list.h -- Fluent Design List widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentList fluent_list.h nanogui/fluent_list.h
 *
 * \brief Fluent Design List widget.
 *
 * A container for FluentListItem widgets with Fluent Design styling.
 */
class NANOGUI_EXPORT FluentList : public Widget {
public:
    FluentList(Widget *parent);

    void draw(NVGcontext *ctx) override;
};

NAMESPACE_END(nanogui)
