/*
 * Meta Editor - Image Tool
 *
 * Click to place image placeholder, then load image via set_image_path().
 * Press 'I' to activate.
 */

#pragma once

#include "../tool.h"
#include <flex.h>
#include <string>

namespace meta_editor {

class ImageTool : public Tool {
public:
    ImageTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Image"; }
    const char* icon() const override { return "image"; }

    void set_pending_image(const std::string& path) { pending_image_path_ = path; }

private:
    flex::Vec2 preview_pos_;
    bool show_preview_ = false;
    std::string pending_image_path_;
    float default_width_ = 200.0f;
    float default_height_ = 150.0f;
};

} // namespace meta_editor
