#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include "backends/tui/init.h"
#include "flex/core/render_value_validation.h"
#include <salts_unicode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace flex {
namespace {

constexpr int kCharWidth = 8;
constexpr int kCharHeight = 16;

class TuiRenderer final : public Renderer {
public:
    explicit TuiRenderer(tui_buffer_t* buffer)
        : buf_(buffer) {}
    explicit TuiRenderer(tui_terminal_t* term)
        : term_(term) {}

    void begin_frame(float w, float h, float) override {
        // A terminal may not be initialized when the Renderer is created.
        // The front/back targets may also be replaced on resize. Borrow the
        // current owner frame buffer, never an old buffer from construction.
        if (term_) buf_ = tui_terminal_buffer(term_);
        frame_w_ = w;
        frame_h_ = h;
        tx_ = ty_ = 0.0f;
        alpha_ = 1.0f;
        clip_ = {0.0f, 0.0f, w, h};
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
            auto& state = save_stack_[--save_depth_];
            tx_ = state.tx;
            ty_ = state.ty;
            alpha_ = state.alpha;
            clip_ = state.clip;
        }
    }

    void reset() override {
        tx_ = 0.0f;
        ty_ = 0.0f;
        alpha_ = 1.0f;
    }

    void set_transform(const Transform& transform) override {
        tx_ = tx(transform);
        ty_ = ty(transform);
    }
    void translate(float x, float y) override {
        tx_ += x;
        ty_ += y;
    }
    void rotate(float) override {}
    void scale(float, float) override {}

    void clip_rect(float x, float y, float w, float h) override {
        const float cx = x + tx_;
        const float cy = y + ty_;
        const float x1 = std::max(clip_.x, cx);
        const float y1 = std::max(clip_.y, cy);
        const float x2 = std::min(clip_.x + clip_.w, cx + w);
        const float y2 = std::min(clip_.y + clip_.h, cy + h);
        clip_ = {x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1)};
    }

    void reset_clip() override { clip_ = {0.0f, 0.0f, frame_w_, frame_h_}; }
    void set_global_alpha(float alpha) override { alpha_ = alpha; }
    void set_shadow(const Shadow& shadow) override {
        if (!render_values::valid_shadow(shadow)) {
            throw std::invalid_argument(
                "Tango renderer shadow values must be finite with non-negative blur");
        }
        if (!shadow.is_none() && shadow.color.a > 0.0f) {
            throw std::logic_error("Tango renderer does not support shadows");
        }
    }
    void clear_shadow() override {}
    void set_blur(const BlurFilter& blur) override {
        if (!render_values::valid_blur(blur)) {
            throw std::invalid_argument(
                "Tango renderer blur radius must be finite and non-negative");
        }
        if (!blur.is_none()) {
            throw std::logic_error("Tango renderer does not support blur filters");
        }
    }
    void clear_blur() override {}
    void fill_path(const std::string&, const Paint&) override {}
    void stroke_path(const std::string&, const Paint&, float) override {}

    void draw_line(float x1, float y1, float x2, float y2,
                   const Paint& paint, float) override {
        if (paint.type == Paint::Type::None) {
            return;
        }

        tui_color_t color = TUI_WHITE;
        if (paint.type == Paint::Type::Solid) {
            color = to_color(paint.color);
        }

        const float ax1 = x1 + tx_;
        const float ay1 = y1 + ty_;
        const float ax2 = x2 + tx_;
        const float ay2 = y2 + ty_;
        const float dx = ax2 - ax1;
        const float dy = ay2 - ay1;
        const int steps = std::max(1, static_cast<int>(std::max(std::abs(dx), std::abs(dy))));

        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            set_braille_dot(ax1 + dx * t, ay1 + dy * t, color);
        }
    }

    void draw_rect(float x, float y, float w, float h, float,
                   const Paint& fill, const Paint& stroke, float stroke_w) override {
        const float cx = x + tx_;
        const float cy = y + ty_;
        if (!intersects(cx, cy, w, h)) {
            return;
        }

        const int c0 = px_to_col_floor(std::max(cx, clip_.x));
        const int r0 = px_to_row_floor(std::max(cy, clip_.y));
        const int c1 = px_to_col_ceil(std::min(cx + w, clip_.x + clip_.w));
        const int r1 = px_to_row_ceil(std::min(cy + h, clip_.y + clip_.h));
        const int cols = c1 - c0;
        const int rows = r1 - r0;
        if (cols < 1 || rows < 1) {
            return;
        }

        tui_color_t fg = TUI_WHITE;
        tui_color_t bg = TUI_BLACK;
        if (fill.type == Paint::Type::Solid) {
            bg = to_color(fill.color);
        }
        if (stroke.type == Paint::Type::Solid) {
            fg = to_color(stroke.color);
        }

        if (fill.type != Paint::Type::None) {
            tui_buffer_fill_color(buf_, c0, r0, cols, rows, bg);
        }
        if (stroke.type != Paint::Type::None && stroke_w > 0.0f && rows >= 2 && cols >= 2) {
            tui_buffer_box(buf_, c0, r0, cols, rows, fg, bg);
        }
    }

    void draw_circle(float cx, float cy, float r,
                     const Paint& fill, const Paint& stroke, float) override {
        const float ax = cx + tx_;
        const float ay = cy + ty_;
        constexpr float aspect = static_cast<float>(kCharHeight) / static_cast<float>(kCharWidth);

        if (fill.type != Paint::Type::None) {
            const tui_color_t color = to_color(fill.color);
            const int c0 = px_to_col(ax - r);
            const int r0 = px_to_row(ay - r / aspect);
            const int c1 = px_to_col(ax + r);
            const int r1 = px_to_row(ay + r / aspect);
            for (int row = r0; row <= r1; ++row) {
                for (int col = c0; col <= c1; ++col) {
                    const float pcx = static_cast<float>(col * kCharWidth) + kCharWidth / 2.0f;
                    const float pcy = static_cast<float>(row * kCharHeight) + kCharHeight / 2.0f;
                    const float dx = pcx - ax;
                    const float dy = (pcy - ay) * aspect;
                    if (dx * dx + dy * dy <= r * r && in_clip(pcx, pcy) &&
                        tui_buffer_in_bounds(buf_, col, row)) {
                        tui_buffer_fill_color(buf_, col, row, 1, 1, color);
                    }
                }
            }
        }

        if (stroke.type != Paint::Type::None) {
            const tui_color_t color = to_color(stroke.color);
            const int steps = std::max(16, static_cast<int>(2.0f * 3.14159f * r / 2.0f));
            for (int i = 0; i < steps; ++i) {
                const float theta = 2.0f * 3.14159f * static_cast<float>(i) / static_cast<float>(steps);
                const float px = ax + r * std::cos(theta);
                const float py = ay + (r / aspect) * std::sin(theta);
                set_braille_dot(px, py, color);
            }
        }
    }

    void draw_ellipse(float cx, float cy, float rx, float ry,
                      const Paint& fill, const Paint& stroke, float) override {
        const float ax = cx + tx_;
        const float ay = cy + ty_;
        constexpr float aspect = static_cast<float>(kCharHeight) / static_cast<float>(kCharWidth);
        const float ry_adj = ry / aspect;

        if (fill.type != Paint::Type::None) {
            const tui_color_t color = to_color(fill.color);
            const int r0 = px_to_row(ay - ry_adj);
            const int r1 = px_to_row(ay + ry_adj);
            for (int row = r0; row <= r1; ++row) {
                const float pcy = static_cast<float>(row * kCharHeight) + kCharHeight / 2.0f;
                const float dy = (pcy - ay) / ry_adj;
                if (std::abs(dy) > 1.0f) {
                    continue;
                }
                const float hw = rx * std::sqrt(1.0f - dy * dy);
                const int c0 = px_to_col(ax - hw);
                const int c1 = px_to_col(ax + hw);
                for (int col = c0; col <= c1; ++col) {
                    const float pcx = static_cast<float>(col * kCharWidth) + kCharWidth / 2.0f;
                    if (in_clip(pcx, pcy) && tui_buffer_in_bounds(buf_, col, row)) {
                        tui_buffer_fill_color(buf_, col, row, 1, 1, color);
                    }
                }
            }
        }

        if (stroke.type != Paint::Type::None) {
            const tui_color_t color = to_color(stroke.color);
            const int steps = std::max(16, static_cast<int>(2.0f * 3.14159f * std::max(rx, ry_adj) / 2.0f));
            for (int i = 0; i < steps; ++i) {
                const float theta = 2.0f * 3.14159f * static_cast<float>(i) / static_cast<float>(steps);
                set_braille_dot(ax + rx * std::cos(theta), ay + ry_adj * std::sin(theta), color);
            }
        }
    }

    void draw_text(const std::string& text, float x, float y,
                   const std::string&, float, bool, const Color& color) override {
        if (!buf_ || text.empty() || clip_.w <= 0.0f || clip_.h <= 0.0f)
            return;

        // Text is emitted as whole terminal cells. A scrolled Markdown
        // child must not paint outside its parent's clip and overwrite the
        // fixed header or footer. Reject partial-height rows and partial
        // double-width graphemes rather than leaking into adjacent cells.
        const int row = px_to_row_floor(y + ty_);
        const int top = std::max(0, px_to_row_ceil(clip_.y));
        const int bottom = std::min(buf_->height,
            px_to_row_floor(clip_.y + clip_.h));
        if (row < top || row >= bottom) return;

        const int left = std::max(0, px_to_col_ceil(clip_.x));
        const int right = std::min(buf_->width,
            px_to_col_floor(clip_.x + clip_.w));
        if (left >= right) return;

        const int start_col = px_to_col_floor(x + tx_);
        const tui_color_t fg = to_color(color);
        // Common fully visible labels can use Tango's already-validated
        // grapheme writer unchanged. UTF-8 bytes upper-bound cell width.
        if (start_col >= left && start_col < right &&
            text.size() <= static_cast<size_t>(right - start_col)) {
            (void)tui_buffer_text(buf_, start_col, row, text.c_str(),
                                  fg, TUI_BLACK);
            return;
        }

        const vstr input = vstr_from_buf(text.data(), text.size());
        size_t offset = 0u;
        vstr cluster;
        // Failure atomicity: validate all graphemes and Tango widths before
        // publishing any clipped cell. Do not introduce a second UTF-8
        // parser or invent an alternate glyph-width policy.
        while (offset < input.len) {
            if (salts_unicode_grapheme_next(input, &offset, &cluster) !=
                    SALTS_UNICODE_OK ||
                cluster.len > TUI_CELL_GRAPHEME_CAPACITY ||
                tui_grapheme_width(cluster.data, cluster.len) < 0)
                return;
        }

        int64_t col = start_col;
        offset = 0u;
        while (offset < input.len && col < right) {
            if (salts_unicode_grapheme_next(input, &offset, &cluster) !=
                    SALTS_UNICODE_OK) return;
            const int width = tui_grapheme_width(cluster.data, cluster.len);
            if (width <= 0) continue;
            if (col >= left && col + width <= right) {
                if (!tui_buffer_set_grapheme(buf_, static_cast<int>(col),
                         row, cluster.data, cluster.len, fg, TUI_BLACK,
                         TUI_STYLE_NONE))
                    return;
            }
            col += width;
        }
    }

    void draw_image(const std::string&, float, float, float, float) override {}
    void draw_svg(const std::string&, float, float, float, float) override {}
    void draw_svg_data(const std::string&, float, float, float, float) override {}

    void clear(const Color& color) override { tui_buffer_clear_color(buf_, to_color(color)); }

    Bounds viewport() const override { return {0.0f, 0.0f, frame_w_, frame_h_}; }
    RendererCapabilities capabilities() const override {
        RendererCapabilities caps;
        caps.retained_mode = false;
        caps.surface_recreation = false;
        caps.path_drawing = false;
        caps.raster_images = false;
        caps.svg_images = false;
        caps.rotation = false;
        caps.scaling = false;
        caps.shadow = false;
        caps.blur = false;
        return caps;
    }

    bool supports_retained_mode() const override { return false; }
    void remove_cached(PaintHandle) override {}
    PaintHandle push_rect(float, float, float, float, float, const Paint&, const Paint&, float,
                          const Transform&, float) override { return nullptr; }
    PaintHandle push_circle(float, float, float, const Paint&, const Paint&, float,
                            const Transform&, float) override { return nullptr; }
    PaintHandle push_ellipse(float, float, float, float, const Paint&, const Paint&, float,
                             const Transform&, float) override { return nullptr; }
    PaintHandle push_polygon(int, float, const Paint&, const Paint&, float,
                             const Transform&, float) override { return nullptr; }
    PaintHandle push_star(int, float, float, const Paint&, const Paint&, float,
                          const Transform&, float) override { return nullptr; }
    PaintHandle push_path(const std::string&, const Paint&, const Paint&, float,
                          const Transform&, float) override { return nullptr; }
    void update_transform(PaintHandle, const Transform&) override {}

private:
    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    struct SaveState {
        float tx = 0.0f;
        float ty = 0.0f;
        float alpha = 1.0f;
        Rect clip{};
    };

    int px_to_col(float px) const { return static_cast<int>(px / kCharWidth); }
    int px_to_row(float py) const { return static_cast<int>(py / kCharHeight); }
    int px_to_col_floor(float px) const { return static_cast<int>(std::floor(px / kCharWidth)); }
    int px_to_row_floor(float py) const { return static_cast<int>(std::floor(py / kCharHeight)); }
    int px_to_col_ceil(float px) const { return static_cast<int>(std::ceil(px / kCharWidth)); }
    int px_to_row_ceil(float py) const { return static_cast<int>(std::ceil(py / kCharHeight)); }

    tui_color_t to_color(const Color& color) const {
        return TUI_COLOR(
            static_cast<uint8_t>(color.r * 255.0f),
            static_cast<uint8_t>(color.g * 255.0f),
            static_cast<uint8_t>(color.b * 255.0f));
    }

    bool in_clip(float px, float py) const {
        return px >= clip_.x && px < clip_.x + clip_.w && py >= clip_.y && py < clip_.y + clip_.h;
    }

    bool intersects(float x, float y, float w, float h) const {
        return !(x + w < clip_.x || x >= clip_.x + clip_.w || y + h < clip_.y ||
                 y >= clip_.y + clip_.h);
    }

    void set_braille_dot(float px, float py, tui_color_t color) {
        if (!in_clip(px, py)) {
            return;
        }
        tui_buffer_braille_set(
            buf_,
            static_cast<int>(px / 4.0f),
            static_cast<int>(py / 4.0f),
            color,
            TUI_BLACK);
    }

    tui_terminal_t* term_ = nullptr;  // borrowed, outlives renderer
    tui_buffer_t* buf_ = nullptr;    // current-frame borrow
    float frame_w_ = 0.0f;
    float frame_h_ = 0.0f;
    float tx_ = 0.0f;
    float ty_ = 0.0f;
    float alpha_ = 1.0f;
    Rect clip_{};
    SaveState save_stack_[32]{};
    int save_depth_ = 0;
};

} // namespace

namespace tui_backend {

void init() {}
void shutdown() {}

bool register_backend() {
    flex::register_renderer_backend(
        RendererBackend::TUI,
        static_cast<RendererFactory>(&flex::create_tui_renderer));
    if (!flex::default_renderer_factory()) {
        flex::set_default_renderer_factory(static_cast<RendererFactory>(&flex::create_tui_renderer));
    }
    return true;
}

std::unique_ptr<Renderer> create_renderer(tui_terminal_t* term) {
    return flex::create_tui_renderer(term);
}

std::unique_ptr<Renderer> create_buffer_renderer(tui_buffer_t* buffer) {
    if (!buffer) return nullptr;
    return std::make_unique<TuiRenderer>(buffer);
}

} // namespace tui_backend

std::unique_ptr<Renderer> create_tui_renderer(tui_terminal_t* term) {
    if (!term) {
        return nullptr;
    }
    return std::make_unique<TuiRenderer>(term);
}

std::unique_ptr<Renderer> create_tui_renderer(CanvasHandle terminal) {
    return create_tui_renderer(static_cast<tui_terminal_t*>(terminal));
}

} // namespace flex
