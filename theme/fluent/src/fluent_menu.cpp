/*
    src/fluent_menu.cpp -- Fluent Design Menu implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_menu.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/layout.h>
#include <nanogui/button.h>

NAMESPACE_BEGIN(nanogui)

class FluentMenu::MenuItem : public Button {
public:
    MenuItem(Widget *parent, const std::string &label, int icon = 0)
        : Button(parent, label, icon) {
        set_fixed_height(48);
        set_icon_position(IconPosition::Left);
    }

    void draw(NVGcontext *ctx) override {
        float x = static_cast<float>(m_pos.x());
        float y = static_cast<float>(m_pos.y());
        float w = static_cast<float>(m_size.x());
        float h = static_cast<float>(m_size.y());

        nvgSave(ctx);

        // Draw hover background
        if (m_mouse_focus) {
            nvgBeginPath(ctx);
            nvgRect(ctx, x, y, w, h);
            nvgFillColor(ctx, nvgRGBAf(0.f, 0.f, 0.f, 0.04f));
            nvgFill(ctx);
        }

        // Draw icon if present
        float content_x = x + 16.f;
        if (m_icon) {
            auto icon = utf8(m_icon);
            nvgFontSize(ctx, 24.f);
            nvgFontFace(ctx, "icons");
            nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(ctx, content_x, y + h * 0.5f, icon.data(), nullptr);
            content_x += 40.f;
        }

        // Draw label
        if (!m_caption.empty()) {
            nvgFontSize(ctx, 16.f);
            nvgFontFace(ctx, "sans");
            nvgFillColor(ctx, nvgRGBAf(0.2f, 0.2f, 0.2f, 1.f));
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(ctx, content_x, y + h * 0.5f, m_caption.c_str(), nullptr);
        }

        nvgRestore(ctx);
    }
};

FluentMenu::FluentMenu(Widget *parent, Widget *anchor)
    : Popup(parent, nullptr) {
    if (anchor)
        set_anchor_pos(anchor->position());
    
    set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 0));
}

void FluentMenu::add_item(const std::string &label, const std::function<void()> &callback, int icon) {
    auto item = new MenuItem(this, label, icon);
    item->set_callback([this, callback]() {
        if (callback)
            callback();
        set_visible(false);
    });
}

void FluentMenu::add_divider() {
    Widget *divider = new Widget(this);
    divider->set_fixed_height(1);
}

void FluentMenu::draw(NVGcontext *ctx) {
    // Draw menu background with shadow
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    nvgSave(ctx);

    // Draw shadow
    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.3f);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);

    NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2.f, w, h, 4.f, 8.f, shadow_outer, shadow_inner);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 4.f, y - 4.f, w + 8.f, h + 8.f, 4.f);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);

    // Draw background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 4.f);
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgFill(ctx);

    nvgRestore(ctx);

    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
