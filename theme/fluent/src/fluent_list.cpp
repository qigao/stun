/*
    src/fluent_list.cpp -- Fluent Design List implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_list.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/layout.h>

NAMESPACE_BEGIN(nanogui)

FluentList::FluentList(Widget *parent) : Widget(parent) {
    set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 0));
}

void FluentList::draw(NVGcontext *ctx) {
    // Draw list background
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(Widget::size().x());
    float h = static_cast<float>(Widget::size().y());

    nvgSave(ctx);

    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgFill(ctx);

    nvgRestore(ctx);

    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
