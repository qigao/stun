/*
    src/fluent_acrylic.cpp -- Fluent Design Acrylic Material implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_acrylic.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <random>

NAMESPACE_BEGIN(nanogui)

FluentAcrylic::FluentAcrylic(Widget *parent, Type type)
    : Widget(parent), m_type(type), m_noise_image(-1) {
    initialize_defaults();
}

void FluentAcrylic::initialize_defaults() {
    // Set defaults based on Fluent Design specifications
    if (m_type == Type::Background) {
        m_tint_opacity = 0.8f;
        m_blur_radius = 30.0f;
        m_noise_opacity = 0.03f;
    } else { // InApp
        m_tint_opacity = 0.7f;
        m_blur_radius = 20.0f;
        m_noise_opacity = 0.02f;
    }

    // Default tint color (will use theme surface color if not set)
    m_tint_color = Color(255, 255, 255, 255);
}

void FluentAcrylic::ensure_noise_texture(NVGcontext *ctx) {
    if (m_noise_image >= 0)
        return;

    // Generate 128x128 noise texture
    const int size = 128;
    std::vector<unsigned char> noise_data(size * size);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (int i = 0; i < size * size; ++i) {
        noise_data[i] = static_cast<unsigned char>(dis(gen));
    }

    m_noise_image = nvgCreateImageRGBA(ctx, size, size, 
                                       NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY,
                                       noise_data.data());
}

void FluentAcrylic::draw(NVGcontext *ctx) {
    if (!m_visible)
        return;

    Widget::draw(ctx);

    ensure_noise_texture(ctx);

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    nvgSave(ctx);

    // Step 1: Draw background with blur effect
    // Note: True gaussian blur requires framebuffer effects
    // This is a simplified version using box shadow
    NVGpaint shadow_paint = nvgBoxGradient(ctx, x, y, w, h,
                                           m_theme->m_window_corner_radius,
                                           m_blur_radius,
                                           Color(0, 0, 0, 20),
                                           Color(0, 0, 0, 0));
    
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, m_theme->m_window_corner_radius);
    nvgFillPaint(ctx, shadow_paint);
    nvgFill(ctx);

    // Step 2: Apply tint color overlay
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, m_theme->m_window_corner_radius);
    nvgFillColor(ctx, Color(m_tint_color.r(), m_tint_color.g(), 
                           m_tint_color.b(), m_tint_opacity));
    nvgFill(ctx);

    // Step 3: Add noise texture overlay
    if (m_noise_image >= 0 && m_noise_opacity > 0.0f) {
        NVGpaint noise_paint = nvgImagePattern(ctx, x, y, 128, 128, 0, 
                                               m_noise_image, m_noise_opacity);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, m_theme->m_window_corner_radius);
        nvgFillPaint(ctx, noise_paint);
        nvgFill(ctx);
    }

    // Step 4: Add subtle border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1, h - 1, 
                   m_theme->m_window_corner_radius);
    nvgStrokeColor(ctx, Color(255, 255, 255, 20));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
