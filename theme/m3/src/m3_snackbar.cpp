/*
    src/m3_snackbar.cpp -- Material Design 3 Snackbar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_snackbar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Snackbar::M3Snackbar(Widget *parent, const std::string &message, 
                       const std::string &action_label)
    : Widget(parent), m_message(message), m_action_label(action_label) {
    set_visible(false);
}

M3Theme *M3Snackbar::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Snackbar::show(float duration) {
    m_duration = duration;
    m_show_time = 0.0f;
    set_visible(true);
    
    // Position at bottom center of parent
    if (m_parent) {
        // Use a default size, will be adjusted in draw
        int width = 400; // Default width
        int height = 48;
        int x = (m_parent->width() - width) / 2;
        int y = m_parent->height() - height - 16;
        set_position(Vector2i(x, y));
        set_size(Vector2i(width, height));
    }
}

void M3Snackbar::dismiss() {
    set_visible(false);
}

bool M3Snackbar::action_at_position(const Vector2i &p) const {
    if (m_action_label.empty())
        return false;

    // Approximate action button area (right side of snackbar)
    Vector2i local = p - m_pos;
    float action_area_width = 100.0f; // Approximate width for action button
    float action_x = m_size.x() - action_area_width;
    
    return local.x() >= action_x && local.x() <= m_size.x() &&
           local.y() >= 0 && local.y() <= m_size.y();
}

bool M3Snackbar::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (button != NANOGUI_MOUSE_BUTTON_LEFT || !down)
        return false;

    if (action_at_position(p)) {
        if (m_action_callback)
            m_action_callback();
        dismiss();
        return true;
    }

    return false;
}

Vector2i M3Snackbar::preferred_size(NVGcontext *ctx) const {
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    
    float message_width = nvgTextBounds(ctx, 0, 0, m_message.c_str(), nullptr, nullptr);
    
    float action_width = 0;
    if (!m_action_label.empty()) {
        nvgFontFace(ctx, "sans-bold");
        action_width = nvgTextBounds(ctx, 0, 0, m_action_label.c_str(), nullptr, nullptr) + 32;
    }
    
    float total_width = message_width + action_width + 48; // 24dp padding on each side
    total_width = std::max(344.0f, std::min(total_width, 672.0f)); // Min/max width
    
    return Vector2i(static_cast<int>(total_width), 48);
}

void M3Snackbar::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme || !m_visible) {
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Small);

    nvgSave(ctx);

    // Draw shadow
    NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2, w, h, corner_radius, 6.0f,
                                     nvgRGBAf(0, 0, 0, 0.25f),
                                     nvgRGBAf(0, 0, 0, 0));
    nvgBeginPath(ctx);
    nvgRect(ctx, x - 6, y - 6, w + 12, h + 14);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);

    // Draw background (inverse surface)
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    nvgFillColor(ctx, theme->inverse_surface());
    nvgFill(ctx);

    // Draw message
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->inverse_on_surface());
    nvgText(ctx, x + 16, y + h * 0.5f, m_message.c_str(), nullptr);

    // Draw action button
    if (!m_action_label.empty()) {
        nvgFontFace(ctx, "sans-bold");
        float action_width = nvgTextBounds(ctx, 0, 0, m_action_label.c_str(), nullptr, nullptr);
        float action_x = x + w - action_width - 24;
        float action_y = y + h * 0.5f;

        // Draw action hover state
        if (m_action_hover) {
            Color state_color = theme->state_layer(theme->inverse_primary(), 0.08f);
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, action_x - 8, y + (h - 32) * 0.5f, 
                          action_width + 16, 32, 4);
            nvgFillColor(ctx, state_color);
            nvgFill(ctx);
        }

        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->inverse_primary());
        nvgText(ctx, action_x, action_y, m_action_label.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
