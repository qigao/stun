/*
    src/fluent_reveal.cpp -- Fluent Design Reveal Highlight implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_reveal.h>
#include <nanogui/opengl.h>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

// Static member initialization
float FluentReveal::s_glow_radius = 100.0f;
float FluentReveal::s_intensity = 0.25f;

void FluentReveal::draw_reveal(NVGcontext *ctx,
                               float x, float y, float w, float h,
                               float cursor_x, float cursor_y,
                               bool is_hovered, bool is_pressed,
                               float corner_radius) {
    if (!is_hovered)
        return;

    nvgSave(ctx);

    // Calculate cursor position relative to widget
    float rel_x = cursor_x - x;
    float rel_y = cursor_y - y;

    // Clamp cursor position to widget bounds
    rel_x = std::clamp(rel_x, 0.f, w);
    rel_y = std::clamp(rel_y, 0.f, h);

    // Calculate distance from cursor to widget center
    float center_x = w * 0.5f;
    float center_y = h * 0.5f;
    float dx = rel_x - center_x;
    float dy = rel_y - center_y;
    float distance = std::sqrt(dx * dx + dy * dy);
    float max_distance = std::sqrt(center_x * center_x + center_y * center_y);

    // Calculate intensity based on distance (closer = brighter)
    float distance_factor = 1.0f - std::min(distance / max_distance, 1.0f);
    float alpha = s_intensity * distance_factor;

    if (is_pressed)
        alpha *= 0.5f; // Reduce intensity when pressed

    // Create radial gradient centered at cursor position
    NVGpaint glow = nvgRadialGradient(ctx, 
                                      x + rel_x, y + rel_y,
                                      0, s_glow_radius,
                                      Color(255, 255, 255, (int)(alpha * 255)),
                                      Color(255, 255, 255, 0));

    // Clip to widget bounds
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    nvgFillPaint(ctx, glow);
    nvgFill(ctx);

    nvgRestore(ctx);
}

void FluentReveal::draw_border_reveal(NVGcontext *ctx,
                                      float x, float y, float w, float h,
                                      float cursor_x, float cursor_y,
                                      bool is_hovered,
                                      float corner_radius) {
    if (!is_hovered)
        return;

    nvgSave(ctx);

    // Calculate cursor position relative to widget
    float rel_x = cursor_x - x;
    float rel_y = cursor_y - y;

    // Find closest point on border
    float border_x = std::clamp(rel_x, 0.f, w);
    float border_y = std::clamp(rel_y, 0.f, h);

    // If cursor is inside, project to nearest edge
    if (rel_x > 0 && rel_x < w && rel_y > 0 && rel_y < h) {
        float dist_left = rel_x;
        float dist_right = w - rel_x;
        float dist_top = rel_y;
        float dist_bottom = h - rel_y;
        
        float min_dist = std::min({dist_left, dist_right, dist_top, dist_bottom});
        
        if (min_dist == dist_left) border_x = 0;
        else if (min_dist == dist_right) border_x = w;
        else if (min_dist == dist_top) border_y = 0;
        else border_y = h;
    }

    // Create gradient along border
    float gradient_size = s_glow_radius * 0.5f;
    NVGpaint border_glow = nvgRadialGradient(ctx,
                                             x + border_x, y + border_y,
                                             0, gradient_size,
                                             Color(255, 255, 255, (int)(s_intensity * 200)),
                                             Color(255, 255, 255, 0));

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1, h - 1, corner_radius);
    nvgStrokePaint(ctx, border_glow);
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
