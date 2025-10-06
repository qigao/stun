/*
    src/fluent_mica.cpp -- Fluent Design Mica Material implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_mica.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <random>

NAMESPACE_BEGIN(nanogui)

FluentMica::FluentMica(Widget *parent)
    : Widget(parent), m_tint_opacity(0.5f), 
      m_luminosity_threshold(0.5f), m_base_image(-1) {
    m_tint_color = Color(240, 240, 240, 255);
}

void FluentMica::ensure_base_texture(NVGcontext *ctx) {
    if (m_base_image >= 0)
        return;

    // Generate a subtle texture pattern (simulating wallpaper sampling)
    const int size = 256;
    std::vector<unsigned char> texture_data(size * size * 4);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(200, 255);
    
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            
            // Create subtle gradient with noise
            float gradient = (float)(x + y) / (size * 2);
            int base = (int)(gradient * 55) + 200;
            int noise = dis(gen) - 227;
            int value = std::clamp(base + noise, 0, 255);
            
            texture_data[idx + 0] = value;     // R
            texture_data[idx + 1] = value;     // G
            texture_data[idx + 2] = value;     // B
            texture_data[idx + 3] = 255;       // A
        }
    }

    m_base_image = nvgCreateImageRGBA(ctx, size, size, 0, texture_data.data());
}

void FluentMica::draw(NVGcontext *ctx) {
    if (!m_visible)
        return;

    Widget::draw(ctx);

    ensure_base_texture(ctx);

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    nvgSave(ctx);

    // Step 1: Draw base texture (simulated wallpaper)
    if (m_base_image >= 0) {
        NVGpaint base_paint = nvgImagePattern(ctx, x, y, 256, 256, 0, 
                                              m_base_image, 0.3f);
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y, w, h);
        nvgFillPaint(ctx, base_paint);
        nvgFill(ctx);
    }

    // Step 2: Apply tint color overlay
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, Color(m_tint_color.r(), m_tint_color.g(), 
                           m_tint_color.b(), m_tint_opacity));
    nvgFill(ctx);

    // Step 3: Add subtle noise for depth
    NVGpaint noise_paint = nvgLinearGradient(ctx, x, y, x, y + h,
                                             Color(255, 255, 255, 5),
                                             Color(0, 0, 0, 5));
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillPaint(ctx, noise_paint);
    nvgFill(ctx);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
