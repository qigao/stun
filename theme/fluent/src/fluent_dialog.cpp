/*
    src/fluent_dialog.cpp -- Fluent Design Dialog implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_dialog.h>
#include <nanogui/fluent_button.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/layout.h>

NAMESPACE_BEGIN(nanogui)

FluentDialog::FluentDialog(Widget *parent, const std::string &title)
    : Window(parent, title) {
    
    set_modal(true);
    set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 24, 16));
    
    // Content area
    m_content = new Widget(this);
    m_content->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 8));
    
    // Actions area
    m_actions = new Widget(this);
    m_actions->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
}

void FluentDialog::add_action(const std::string &label, const std::function<void()> &callback) {
    auto button = new FluentButton(m_actions, label, 0, FluentButton::Style::Text);
    button->set_callback([this, callback]() {
        if (callback)
            callback();
        dispose();
    });
}

void FluentDialog::draw(NVGcontext *ctx) {
    // Draw backdrop
    if (m_modal) {
        float x = static_cast<float>(m_parent->position().x());
        float y = static_cast<float>(m_parent->position().y());
        float w = static_cast<float>(m_parent->width());
        float h = static_cast<float>(m_parent->height());
        
        nvgSave(ctx);
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y, w, h);
        nvgFillColor(ctx, nvgRGBAf(0.f, 0.f, 0.f, 0.5f));
        nvgFill(ctx);
        nvgRestore(ctx);
    }
    
    // Draw dialog with enhanced shadow
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(Widget::size().x());
    float h = static_cast<float>(Widget::size().y());

    nvgSave(ctx);

    // Draw shadow
    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.4f);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);

    NVGpaint shadow = nvgBoxGradient(ctx, x, y + 4.f, w, h, 8.f, 16.f, shadow_outer, shadow_inner);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 8.f, y - 8.f, w + 16.f, h + 16.f, 8.f);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);

    // Draw background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 8.f);
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgFill(ctx);

    nvgRestore(ctx);

    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
