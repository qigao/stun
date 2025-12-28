/*
 * Meta Editor - Floating Context Toolbar
 *
 * Appears below selection with quick style controls:
 * [Fill] [Stroke] [Width] [Rough]
 */

#pragma once

#include <flex.h>
#include <functional>

namespace meta_editor {

class Canvas;
class SelectionManager;

// Quick color palette (12 colors)
static const flex::Color QUICK_COLORS[12] = {
    {0.0f, 0.0f, 0.0f, 1.0f},       // Black
    {0.5f, 0.5f, 0.5f, 1.0f},       // Gray
    {1.0f, 1.0f, 1.0f, 1.0f},       // White
    {0.9f, 0.3f, 0.3f, 1.0f},       // Red
    {0.3f, 0.7f, 0.3f, 1.0f},       // Green
    {0.3f, 0.5f, 0.9f, 1.0f},       // Blue
    {0.95f, 0.6f, 0.2f, 1.0f},      // Orange
    {0.7f, 0.4f, 0.9f, 1.0f},       // Purple
    {0.3f, 0.8f, 0.8f, 1.0f},       // Cyan
    {1.0f, 0.8f, 0.8f, 1.0f},       // Pastel Red
    {0.8f, 1.0f, 0.8f, 1.0f},       // Pastel Green
    {0.8f, 0.8f, 1.0f, 1.0f},       // Pastel Blue
};

class ContextToolbar {
public:
    ContextToolbar(Canvas* canvas, SelectionManager* selection);

    void update();
    void render(flex::Renderer& renderer);

    bool handle_click(float x, float y);

    bool is_visible() const { return visible_; }

private:
    Canvas* canvas_;
    SelectionManager* selection_;

    bool visible_ = false;
    float toolbar_x_ = 0;
    float toolbar_y_ = 0;
    float toolbar_width_ = 200;
    float toolbar_height_ = 36;

    // Popup state
    bool fill_picker_open_ = false;
    bool stroke_picker_open_ = false;

    // Current values from selection
    flex::Color current_fill_ = {0.5f, 0.7f, 0.9f, 1.0f};
    flex::Color current_stroke_ = {0.0f, 0.0f, 0.0f, 1.0f};
    float current_stroke_width_ = 2.0f;
    bool current_rough_ = true;

    void update_from_selection();
    void update_position();

    void render_color_swatch(flex::Renderer& r, float x, float y, float size,
                             const flex::Color& color, bool is_stroke);
    void render_color_picker(flex::Renderer& r, float x, float y, bool for_stroke);
    void render_width_control(flex::Renderer& r, float x, float y);
    void render_rough_toggle(flex::Renderer& r, float x, float y);

    bool hit_test_swatch(float mx, float my, float sx, float sy, float size);
    int hit_test_color_picker(float mx, float my, float px, float py);
};

} // namespace meta_editor
