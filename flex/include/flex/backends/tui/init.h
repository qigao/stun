/*
 * Flex Engine - TUI Backend (using tango library)
 *
 * Renders flex scenes to terminal using tango's C API.
 */

#pragma once

#include <flex/runtime/renderer.h>
#include <tui.h>
#include <memory>
#include <cmath>
#include <cstring>
#include <cctype>

namespace flex {
namespace tui_backend {

constexpr int CHAR_WIDTH = 8;
constexpr int CHAR_HEIGHT = 16;

class TuiRenderer : public Renderer {
public:
    explicit TuiRenderer(tui_terminal_t* term)
        : term_(term), buf_(tui_terminal_buffer(term)) {}

    int px_to_col(float px) const { return static_cast<int>(px / CHAR_WIDTH); }
    int px_to_row(float py) const { return static_cast<int>(py / CHAR_HEIGHT); }
    int px_to_col_floor(float px) const { return static_cast<int>(std::floor(px / CHAR_WIDTH)); }
    int px_to_row_floor(float py) const { return static_cast<int>(std::floor(py / CHAR_HEIGHT)); }
    int px_to_col_ceil(float px) const { return static_cast<int>(std::ceil(px / CHAR_WIDTH)); }
    int px_to_row_ceil(float py) const { return static_cast<int>(std::ceil(py / CHAR_HEIGHT)); }

    tui_color_t to_color(const Color& c) const {
        return TUI_COLOR(
            static_cast<uint8_t>(c.r * 255),
            static_cast<uint8_t>(c.g * 255),
            static_cast<uint8_t>(c.b * 255)
        );
    }

    void begin_frame(float w, float h, float) override {
        frame_w_ = w;
        frame_h_ = h;
        tx_ = ty_ = 0;
        alpha_ = 1.0f;
        clip_ = {0, 0, w, h};
        save_depth_ = 0;
    }

    void end_frame() override {}
    void set_retained_mode(bool) override {}

    void save() override {
        if (save_depth_ < 32) {
            save_stack_[save_depth_++] = {tx_, ty_, alpha_, clip_};
        }
    }

    void restore() override {
        if (save_depth_ > 0) {
            auto& s = save_stack_[--save_depth_];
            tx_ = s.tx; ty_ = s.ty; alpha_ = s.alpha; clip_ = s.clip;
        }
    }

    void reset() override { tx_ = ty_ = 0; alpha_ = 1.0f; }
    void set_transform(const Transform&) override {}
    void translate(float x, float y) override { tx_ += x; ty_ += y; }
    void rotate(float) override {}
    void scale(float, float) override {}

    void clip_rect(float x, float y, float w, float h) override {
        float cx = x + tx_, cy = y + ty_;
        float x1 = std::max(clip_.x, cx);
        float y1 = std::max(clip_.y, cy);
        float x2 = std::min(clip_.x + clip_.w, cx + w);
        float y2 = std::min(clip_.y + clip_.h, cy + h);
        clip_ = {x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1)};
    }

    void reset_clip() override { clip_ = {0, 0, frame_w_, frame_h_}; }
    void set_global_alpha(float a) override { alpha_ = a; }
    void set_shadow(const Shadow&) override {}
    void clear_shadow() override {}
    void set_blur(const BlurFilter&) override {}
    void clear_blur() override {}
    void fill_path(const std::string&, const Paint&) override {}
    void stroke_path(const std::string&, const Paint&, float) override {}

    void draw_rect(float x, float y, float w, float h, float,
                   const Paint& fill, const Paint& stroke, float stroke_w) override {
        float cx = x + tx_, cy = y + ty_;
        if (!intersects(cx, cy, w, h)) return;

        int c0 = px_to_col_floor(std::max(cx, clip_.x));
        int r0 = px_to_row_floor(std::max(cy, clip_.y));
        int c1 = px_to_col_ceil(std::min(cx + w, clip_.x + clip_.w));
        int r1 = px_to_row_ceil(std::min(cy + h, clip_.y + clip_.h));
        int cols = c1 - c0, rows = r1 - r0;
        if (cols < 1 || rows < 1) return;

        tui_color_t fg = TUI_WHITE, bg = TUI_BLACK;
        if (fill.type == Paint::Type::Solid) bg = to_color(fill.color);
        if (stroke.type == Paint::Type::Solid) fg = to_color(stroke.color);

        if (fill.type != Paint::Type::None) {
            tui_buffer_fill_color(buf_, c0, r0, cols, rows, bg);
        }
        if (stroke.type != Paint::Type::None && stroke_w > 0 && rows >= 2 && cols >= 2) {
            tui_buffer_box(buf_, c0, r0, cols, rows, fg, bg);
        }
    }

    void draw_circle(float cx, float cy, float r,
                     const Paint& fill, const Paint& stroke, float) override {
        float ax = cx + tx_, ay = cy + ty_;
        constexpr float aspect = static_cast<float>(CHAR_HEIGHT) / CHAR_WIDTH;

        if (fill.type != Paint::Type::None) {
            tui_color_t c = to_color(fill.color);
            int c0 = px_to_col(ax - r), r0 = px_to_row(ay - r / aspect);
            int c1 = px_to_col(ax + r), r1 = px_to_row(ay + r / aspect);
            for (int row = r0; row <= r1; ++row) {
                for (int col = c0; col <= c1; ++col) {
                    float pcx = col * CHAR_WIDTH + CHAR_WIDTH / 2.0f;
                    float pcy = row * CHAR_HEIGHT + CHAR_HEIGHT / 2.0f;
                    float dx = pcx - ax, dy = (pcy - ay) * aspect;
                    if (dx*dx + dy*dy <= r*r && in_clip(pcx, pcy) && tui_buffer_in_bounds(buf_, col, row)) {
                        tui_buffer_fill_color(buf_, col, row, 1, 1, c);
                    }
                }
            }
        }
        if (stroke.type != Paint::Type::None) {
            tui_color_t c = to_color(stroke.color);
            int steps = std::max(16, static_cast<int>(2 * 3.14159f * r / 2.0f));
            for (int i = 0; i < steps; ++i) {
                float theta = 2.0f * 3.14159f * i / steps;
                float px = ax + r * std::cos(theta);
                float py = ay + (r / aspect) * std::sin(theta);
                set_braille_dot(px, py, c);
            }
        }
    }

    void draw_ellipse(float cx, float cy, float rx, float ry,
                      const Paint& fill, const Paint& stroke, float) override {
        float ax = cx + tx_, ay = cy + ty_;
        constexpr float aspect = static_cast<float>(CHAR_HEIGHT) / CHAR_WIDTH;
        float ry_adj = ry / aspect;

        if (fill.type != Paint::Type::None) {
            tui_color_t c = to_color(fill.color);
            int r0 = px_to_row(ay - ry_adj), r1 = px_to_row(ay + ry_adj);
            for (int row = r0; row <= r1; ++row) {
                float pcy = row * CHAR_HEIGHT + CHAR_HEIGHT / 2.0f;
                float dy = (pcy - ay) / ry_adj;
                if (std::abs(dy) > 1.0f) continue;
                float hw = rx * std::sqrt(1.0f - dy * dy);
                int c0 = px_to_col(ax - hw), c1 = px_to_col(ax + hw);
                for (int col = c0; col <= c1; ++col) {
                    float pcx = col * CHAR_WIDTH + CHAR_WIDTH / 2.0f;
                    if (in_clip(pcx, pcy) && tui_buffer_in_bounds(buf_, col, row)) {
                        tui_buffer_fill_color(buf_, col, row, 1, 1, c);
                    }
                }
            }
        }
        if (stroke.type != Paint::Type::None) {
            tui_color_t c = to_color(stroke.color);
            int steps = std::max(16, static_cast<int>(2 * 3.14159f * std::max(rx, ry_adj) / 2.0f));
            for (int i = 0; i < steps; ++i) {
                float theta = 2.0f * 3.14159f * i / steps;
                set_braille_dot(ax + rx * std::cos(theta), ay + ry_adj * std::sin(theta), c);
            }
        }
    }

    void draw_text(const std::string& text, float x, float y,
                   const std::string&, float, bool, const Color& color) override {
        int col = px_to_col(x + tx_), row = px_to_row(y + ty_);
        tui_color_t fg = to_color(color);
        tui_buffer_text(buf_, col, row, text.c_str(), fg, TUI_BLACK);
    }

    void draw_image(const std::string&, float, float, float, float) override {}
    void draw_svg(const std::string&, float, float, float, float) override {}
    void draw_svg_data(const std::string&, float, float, float, float) override {}

    void clear(const Color& c) override {
        tui_buffer_clear_color(buf_, to_color(c));
    }

    Bounds viewport() const override { return {0, 0, frame_w_, frame_h_}; }

    bool supports_retained_mode() const override { return false; }
    void remove_cached(PaintHandle) override {}
    PaintHandle push_rect(float, float, float, float, float, const Paint&, const Paint&, float, const Transform&, float) override { return nullptr; }
    PaintHandle push_circle(float, float, float, const Paint&, const Paint&, float, const Transform&, float) override { return nullptr; }
    PaintHandle push_ellipse(float, float, float, float, const Paint&, const Paint&, float, const Transform&, float) override { return nullptr; }
    PaintHandle push_polygon(int, float, const Paint&, const Paint&, float, const Transform&, float) override { return nullptr; }
    PaintHandle push_star(int, float, float, const Paint&, const Paint&, float, const Transform&, float) override { return nullptr; }
    PaintHandle push_path(const std::string&, const Paint&, const Paint&, float, const Transform&, float) override { return nullptr; }
    void update_transform(PaintHandle, const Transform&) override {}

private:
    struct Rect { float x, y, w, h; };
    struct SaveState { float tx, ty, alpha; Rect clip; };

    bool in_clip(float px, float py) const {
        return px >= clip_.x && px < clip_.x + clip_.w && py >= clip_.y && py < clip_.y + clip_.h;
    }

    bool intersects(float x, float y, float w, float h) const {
        return !(x + w < clip_.x || x >= clip_.x + clip_.w || y + h < clip_.y || y >= clip_.y + clip_.h);
    }

    void set_braille_dot(float px, float py, tui_color_t color) {
        if (!in_clip(px, py)) return;
        tui_buffer_braille_set(buf_, static_cast<int>(px / 4), static_cast<int>(py / 4), color, TUI_BLACK);
    }

    tui_terminal_t* term_;
    tui_buffer_t* buf_;
    float frame_w_ = 0, frame_h_ = 0;
    float tx_ = 0, ty_ = 0, alpha_ = 1.0f;
    Rect clip_{0, 0, 0, 0};
    SaveState save_stack_[32];
    int save_depth_ = 0;
};

inline std::unique_ptr<Renderer> create_renderer(tui_terminal_t* term) {
    return std::make_unique<TuiRenderer>(term);
}

} // namespace tui_backend
} // namespace flex
