#pragma once

#include <flex/runtime/renderer.h>
#include <tui/terminal.h>
#include <tui/buffer.h>
#include <arena_buffer.h>
#include <memory>
#include <vector>
#include <cmath>
#include <cctype>

namespace flex {
namespace tui_backend {

// Character cell dimensions (approximate pixel size)
constexpr int CHAR_WIDTH = 8;
constexpr int CHAR_HEIGHT = 16;

class TuiRenderer : public Renderer {
public:
    explicit TuiRenderer(::tui::Terminal& term) 
        : term_(term), buf_(term.buffer()) {
        turbo_arena_init(&arena_, 4096);
    }

    ~TuiRenderer() override {
        turbo_arena_free(&arena_);
    }
    
    // Coordinate conversion
    int px_to_col(float px) const { return static_cast<int>(px / CHAR_WIDTH); }
    int px_to_row(float py) const { return static_cast<int>(py / CHAR_HEIGHT); }
    int px_to_cols(float pw) const { return std::max(1, static_cast<int>(pw / CHAR_WIDTH)); }
    int px_to_rows(float ph) const { return std::max(1, static_cast<int>(ph / CHAR_HEIGHT)); }
    
    ::tui::Color to_color(const Color& c) const {
        return ::tui::Color{
            static_cast<uint8_t>(c.r * 255),
            static_cast<uint8_t>(c.g * 255),
            static_cast<uint8_t>(c.b * 255)
        };
    }

    // ========== Frame Control ==========
    void begin_frame(float w, float h, float) override {
        frame_w_ = w; frame_h_ = h;
        tx_ = ty_ = 0;
        alpha_ = 1.0f;
        turbo_arena_reset(&arena_);
        save_stack_ptr_ = 0;
        save_stack_capacity_ = 0;
        save_stack_data_ = nullptr;
        
        clip_rect_ = {0, 0, w, h};
    }
    
    void end_frame() override {}
    
    void set_retained_mode(bool) override {}

    // ========== Types ==========
    struct Rect {
        float x, y, w, h;
        
        bool contains(float px, float py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
        
        void intersect(float ox, float oy, float ow, float oh) {
            float x1 = std::max(x, ox);
            float y1 = std::max(y, oy);
            float x2 = std::min(x + w, ox + ow);
            float y2 = std::min(y + h, oy + oh);
            
            if (x2 > x1 && y2 > y1) {
                x = x1; y = y1;
                w = x2 - x1; h = y2 - y1;
            } else {
                w = 0; h = 0;
            }
        }
    };

    // ========== Transform ==========
    struct SaveState { 
        float tx, ty, alpha; 
        Rect clip_rect;
    };

    void save() override {
        if (save_stack_ptr_ >= save_stack_capacity_) {
            size_t new_cap = save_stack_capacity_ == 0 ? 8 : save_stack_capacity_ * 2;
            SaveState* new_data = (SaveState*)turbo_arena_alloc(&arena_, sizeof(SaveState) * new_cap);
            if (save_stack_data_ && save_stack_ptr_ > 0) {
                memcpy(new_data, save_stack_data_, sizeof(SaveState) * save_stack_ptr_);
            }
            save_stack_data_ = new_data;
            save_stack_capacity_ = new_cap;
        }
        save_stack_data_[save_stack_ptr_++] = {tx_, ty_, alpha_, clip_rect_};
    }
    
    void restore() override {
        if (save_stack_ptr_ > 0) {
            auto& s = save_stack_data_[--save_stack_ptr_];
            tx_ = s.tx; ty_ = s.ty; alpha_ = s.alpha;
            clip_rect_ = s.clip_rect;
        }
    }
    
    void reset() override { tx_ = ty_ = 0; alpha_ = 1.0f; }
    void set_transform(const Transform&) override {}
    void translate(float x, float y) override { tx_ += x; ty_ += y; }
    void rotate(float) override {}
    void scale(float, float) override {}

    // ========== Clipping ==========
    
    Rect clip_rect_{0, 0, 0, 0};

    void clip_rect(float x, float y, float w, float h) override {
        // Transform clip rect to world space? 
        // Renderer interface usually implies clip rect is in current transform space, 
        // but for TUI simplicity we often treat it as screen space aligned or apply transform.
        // Let's apply translation.
        float cx = x + tx_;
        float cy = y + ty_;
        clip_rect_.intersect(cx, cy, w, h);
    }
    
    void reset_clip() override {
        clip_rect_ = {0, 0, frame_w_, frame_h_};
    }

    // ========== Alpha ==========
    void set_global_alpha(float a) override { alpha_ = a; }

    // ========== Effects (not supported but required) ==========
    void set_shadow(const Shadow&) override {}
    void clear_shadow() override {}
    void set_blur(const BlurFilter&) override {}
    void clear_blur() override {}

    // ========== Drawing ==========
    void fill_path(const std::string&, const Paint&) override {}
    void stroke_path(const std::string&, const Paint&, float) override {}

    // ========== Quantization Helpers ==========
    // Improved quantization using floor/ceil strategy to prevent gaps/overlaps
    int px_to_col_floor(float px) const { return static_cast<int>(std::floor(px / CHAR_WIDTH)); }
    int px_to_row_floor(float py) const { return static_cast<int>(std::floor(py / CHAR_HEIGHT)); }
    int px_to_col_ceil(float px) const { return static_cast<int>(std::ceil(px / CHAR_WIDTH)); }
    int px_to_row_ceil(float py) const { return static_cast<int>(std::ceil(py / CHAR_HEIGHT)); }

    // ========== Braille Graphics ==========
    // Sets a dot in the 2x4 braille sub-grid
    void set_dot(float px, float py, const ::tui::Color& color) {
        // Clipping check
        if (!clip_rect_.contains(px, py)) return;

        int col = px_to_col(px);
        int row = px_to_row(py);
        
        if (!buf_.in_bounds(col, row)) return;
        
        // ... rest of implementation ...
        int dot_x = (static_cast<int>(px) % CHAR_WIDTH) / 4;
        int dot_y = (static_cast<int>(py) % CHAR_HEIGHT) / 4;
        
        // Clamp to valid range
        if (dot_x < 0) dot_x = 0; if (dot_x > 1) dot_x = 1;
        if (dot_y < 0) dot_y = 0; if (dot_y > 3) dot_y = 3;

        static const int masks[4][2] = {
            {0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}
        };
        
        int mask = masks[dot_y][dot_x];
        ::tui::Cell& cell = buf_.at(col, row);
        
        if (cell.ch >= 0x2800 && cell.ch <= 0x28FF) {
            cell.ch |= mask;
            cell.fg = color; 
        } else if (cell.ch == ' ' || cell.ch == 0) {
            cell.ch = 0x2800 | mask;
            cell.fg = color;
        } else {
             cell.ch = 0x2800 | mask;
             cell.fg = color;
        }
    }

    void draw_line_braille(float x1, float y1, float x2, float y2, const ::tui::Color& color) {
        // Bresenham's algorithm adapted for sub-pixel coords (approximate using dots)
        // Step size: 4px (dot size)
        float dx = x2 - x1;
        float dy = y2 - y1;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 0.1f) return;
        
        int steps = static_cast<int>(dist / 2.0f); // 2px step for smoothness
        if (steps < 1) steps = 1;
        
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / steps;
            set_dot(x1 + dx * t, y1 + dy * t, color);
        }
    }

    void draw_rect(float x, float y, float w, float h, float r,
                   const Paint& fill, const Paint& stroke, float stroke_w) override {
        // Apply transform
        float cur_x = x + tx_;
        float cur_y = y + ty_;
        
        // Simple clipping: intersect with clip_rect_
        Rect r_bounds = {cur_x, cur_y, w, h};
        if (r_bounds.x + r_bounds.w < clip_rect_.x || r_bounds.x >= clip_rect_.x + clip_rect_.w ||
            r_bounds.y + r_bounds.h < clip_rect_.y || r_bounds.y >= clip_rect_.y + clip_rect_.h) {
            return; // Fully out
        }
        
        // Logical bounds (snapped)
        int col_start = px_to_col_floor(std::max(cur_x, clip_rect_.x));
        int row_start = px_to_row_floor(std::max(cur_y, clip_rect_.y));
        int col_end = px_to_col_ceil(std::min(cur_x + w, clip_rect_.x + clip_rect_.w));
        int row_end = px_to_row_ceil(std::min(cur_y + h, clip_rect_.y + clip_rect_.h));
        
        int cols = std::max(0, col_end - col_start);
        int rows = std::max(0, row_end - row_start);
        
        if (cols < 1 || rows < 1) return;
        
        ::tui::Color fg = ::tui::Color::white();
        ::tui::Color bg = ::tui::Color::black();
        
        if (fill.type == Paint::Type::Solid) bg = to_color(fill.color);
        if (stroke.type == Paint::Type::Solid) fg = to_color(stroke.color);
        
        // Fill
        if (fill.type != Paint::Type::None) {
            buf_.fill(col_start, row_start, cols, rows, bg);
        }
        
        // Border
        if (stroke.type != Paint::Type::None && stroke_w > 0) {
            // Note: Clipping borders correctly is tricky with simple box chars. 
            // We reuse standard logic but it might draw partial boxes if we are not careful.
            // For TUI, it's often acceptable to just draw the intersection.
            if (rows >= 2 && cols >= 2) {
                // Only draw full box if largely unclipped? 
                // Let's just draw the box in the clipped region.
                // But `buf_.box` draws borders at edges of the region. 
                // If we pass the clipped region, the borders will be drawn *at the clip edge*, 
                // which might visually close the box incorrectly.
                
                // For correct rendering, we should draw to full extent but check every cell against clip.
                // `buf_.box` doesn't support per-cell check easily without modification.
                // However, for performance, we stick to region fill for `fill`.
                // For `box`, let's do a manual implementation that checks bounds.
                
                int full_col_start = px_to_col_floor(cur_x);
                int full_row_start = px_to_row_floor(cur_y);
                int full_col_end = px_to_col_ceil(cur_x + w);
                int full_row_end = px_to_row_ceil(cur_y + h);
                int full_w = full_col_end - full_col_start;
                int full_h = full_row_end - full_row_start;
                
                // We iterate the perimeter of the FULL box, but only set if inside clip_rect
                // This is slow but correct. 
                // Optimization: Iterate only intersection? 
                // Let's rely on buf_.set bounds check? No, buf bounds are screen bounds. 
                // We need clip bounds.
                
                auto safe_set = [&](int c, int r, char32_t ch) {
                    float px = c * CHAR_WIDTH + CHAR_WIDTH/2.0f;
                    float py = r * CHAR_HEIGHT + CHAR_HEIGHT/2.0f;
                    if (clip_rect_.contains(px, py)) {
                        buf_.set(c, r, ::tui::Cell{ch, fg, bg});
                    }
                };

                // Top/Bottom
                for (int i = 0; i < full_w; ++i) {
                     safe_set(full_col_start + i, full_row_start, (i==0?U'┌':(i==full_w-1?U'┐':U'─')));
                     safe_set(full_col_start + i, full_row_end-1, (i==0?U'└':(i==full_w-1?U'┘':U'─')));
                }
                // Left/Right
                 for (int i = 1; i < full_h - 1; ++i) {
                     safe_set(full_col_start, full_row_start + i, U'│');
                     safe_set(full_col_end - 1, full_row_start + i, U'│');
                }
            }
        }
    }

    void draw_circle(float cx, float cy, float r,
                     const Paint& fill, const Paint& stroke, float stroke_w) override {
        float abs_cx = cx + tx_;
        float abs_cy = cy + ty_;
        
        // Terminal chars are not square (8x16), so we need aspect ratio correction
        constexpr float aspect = static_cast<float>(CHAR_HEIGHT) / CHAR_WIDTH;  // 2.0

        // Fill using block characters
        if (fill.type != Paint::Type::None) {
            ::tui::Color c = to_color(fill.color);
            int col_min = px_to_col(abs_cx - r);
            int row_min = px_to_row(abs_cy - r / aspect);
            int col_max = px_to_col(abs_cx + r);
            int row_max = px_to_row(abs_cy + r / aspect);

            for (int y = row_min; y <= row_max; ++y) {
                for (int x = col_min; x <= col_max; ++x) {
                    float center_px_x = x * CHAR_WIDTH + CHAR_WIDTH / 2.0f;
                    float center_px_y = y * CHAR_HEIGHT + CHAR_HEIGHT / 2.0f;
                    
                    // Scale Y distance by aspect ratio for circular appearance
                    float dx = center_px_x - abs_cx;
                    float dy = (center_px_y - abs_cy) * aspect;
                    float dist_sq = dx * dx + dy * dy;
                    
                    if (dist_sq <= r * r) {
                        if (buf_.in_bounds(x, y) && clip_rect_.contains(center_px_x, center_px_y)) {
                            buf_.at(x, y).bg = c;
                        }
                    }
                }
            }
        }

        // Stroke using Braille (high res)
        if (stroke.type != Paint::Type::None) {
            ::tui::Color c = to_color(stroke.color);
            constexpr float PI = 3.14159265358979323846f;
            int steps = static_cast<int>(2 * PI * r / 2.0f);
            if (steps < 16) steps = 16;

            for (int i = 0; i < steps; ++i) {
                float theta = 2.0f * PI * float(i) / float(steps);
                float px = abs_cx + r * std::cos(theta);
                float py = abs_cy + (r / aspect) * std::sin(theta);  // Scale Y for aspect
                set_dot(px, py, c);
            }
        }
    }

    void draw_ellipse(float cx, float cy, float rx, float ry,
                      const Paint& fill, const Paint& stroke, float sw) override {
        float abs_cx = cx + tx_;
        float abs_cy = cy + ty_;
        
        // Terminal chars are not square (8x16), apply aspect ratio correction
        constexpr float aspect = static_cast<float>(CHAR_HEIGHT) / CHAR_WIDTH;  // 2.0
        float ry_adjusted = ry / aspect;
        
        // Fill using scanline algorithm
        if (fill.type != Paint::Type::None) {
            ::tui::Color c = to_color(fill.color);
            int row_min = px_to_row(abs_cy - ry_adjusted);
            int row_max = px_to_row(abs_cy + ry_adjusted);
            
            for (int row = row_min; row <= row_max; ++row) {
                float cell_y = row * CHAR_HEIGHT + CHAR_HEIGHT / 2.0f;
                float dy = (cell_y - abs_cy) / ry_adjusted;
                if (std::abs(dy) > 1.0f) continue;
                
                // x² / rx² + y² / ry² = 1  =>  x = rx * sqrt(1 - (y/ry)²)
                float half_width = rx * std::sqrt(1.0f - dy * dy);
                int col_min = px_to_col(abs_cx - half_width);
                int col_max = px_to_col(abs_cx + half_width);
                
                for (int col = col_min; col <= col_max; ++col) {
                    float cell_x = col * CHAR_WIDTH + CHAR_WIDTH / 2.0f;
                    if (clip_rect_.contains(cell_x, cell_y) && buf_.in_bounds(col, row)) {
                        buf_.at(col, row).bg = c;
                    }
                }
            }
        }

        // Stroke with Braille
        if (stroke.type != Paint::Type::None) {
            ::tui::Color c = to_color(stroke.color);
            constexpr float PI = 3.14159265358979323846f;
            int steps = static_cast<int>(2 * PI * std::max(rx, ry_adjusted) / 2.0f);
            if (steps < 16) steps = 16;

            for (int i = 0; i < steps; ++i) {
                float theta = 2.0f * PI * float(i) / float(steps);
                float px = abs_cx + rx * std::cos(theta);
                float py = abs_cy + ry_adjusted * std::sin(theta);
                set_dot(px, py, c);
            }
        }
    }
    // Helper to decode one UTF-8 codepoint
    static uint32_t decode_utf8(const char*& p, const char* end) {
        if (p >= end) return 0;
        unsigned char c1 = (unsigned char)*p++;
        if (c1 < 0x80) return c1;
        if ((c1 & 0xE0) == 0xC0) {
            if (p >= end) return c1;
            unsigned char c2 = (unsigned char)*p++;
            return ((c1 & 0x1F) << 6) | (c2 & 0x3F);
        }
        if ((c1 & 0xF0) == 0xE0) {
            if (p + 1 >= end) return c1;
            unsigned char c2 = (unsigned char)*p++;
            unsigned char c3 = (unsigned char)*p++;
            return ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        if ((c1 & 0xF8) == 0xF0) {
            if (p + 2 >= end) return c1;
            unsigned char c2 = (unsigned char)*p++;
            unsigned char c3 = (unsigned char)*p++;
            unsigned char c4 = (unsigned char)*p++;
            return ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
        }
        return c1;
    }

    void parse_sgr(const char*& p, const char* end, ::tui::Color& fg, ::tui::Color& bg, ::tui::Style& style, const ::tui::Color& base_fg, bool& bg_set) {
        int args[16];
        int arg_count = 0;
        
        while (p < end && *p != 'm' && arg_count < 16) {
            if (isdigit(*p)) {
                int val = 0;
                while (p < end && isdigit(*p)) {
                    val = val * 10 + (*p - '0');
                    p++;
                }
                args[arg_count++] = val;
            } else if (*p == ';') {
                if (p + 1 < end && (*p + 1) == ';') {
                    args[arg_count++] = 0; // Double semicolon
                } else if (p + 1 == end || *(p + 1) == 'm') {
                    args[arg_count++] = 0; // Trailing semicolon
                }
                p++;
            } else {
                p++;
            }
        }
        if (p < end && *p == 'm') p++;

        if (arg_count == 0) {
            fg = base_fg; bg = ::tui::Color::black(); style = ::tui::Style::None;
            bg_set = false;
            return;
        }

        for (int i = 0; i < arg_count; ++i) {
            int code = args[i];
            if (code == 0) { fg = base_fg; bg = ::tui::Color::black(); style = ::tui::Style::None; bg_set = false; }
            else if (code == 1) { style = style | ::tui::Style::Bold; }
            else if (code == 2) { style = style | ::tui::Style::Dim; }
            else if (code == 4) { style = style | ::tui::Style::Underline; }
            else if (code == 7) { style = style | ::tui::Style::Reverse; }
            else if (code >= 30 && code <= 37) {
                static const ::tui::Color table[] = {
                    {0,0,0}, {205,0,0}, {0,205,0}, {205,205,0},
                    {0,0,238}, {205,0,205}, {0,205,205}, {229,229,229}
                };
                fg = table[code - 30];
            } else if (code == 38 && i + 1 < arg_count) {
                if (args[i+1] == 2 && i + 4 < arg_count) {
                    fg = { (uint8_t)args[i+2], (uint8_t)args[i+3], (uint8_t)args[i+4] };
                    i += 4;
                } else if (args[i+1] == 5 && i + 2 < arg_count) {
                    i += 2;
                }
            } else if (code == 39) { fg = base_fg; }
            else if (code >= 40 && code <= 47) {
                static const ::tui::Color table[] = {
                    {0,0,0}, {205,0,0}, {0,205,0}, {205,205,0},
                    {0,0,238}, {205,0,205}, {0,205,205}, {229,229,229}
                };
                bg = table[code - 40];
                bg_set = true;
            } else if (code == 48 && i + 1 < arg_count) {
                if (args[i+1] == 2 && i + 4 < arg_count) {
                    bg = { (uint8_t)args[i+2], (uint8_t)args[i+3], (uint8_t)args[i+4] };
                    i += 4;
                    bg_set = true;
                } else if (args[i+1] == 5 && i + 2 < arg_count) {
                    i += 2;
                }
            } else if (code == 49) { bg = ::tui::Color::black(); bg_set = false; }
            else if (code >= 90 && code <= 97) {
                static const ::tui::Color table[] = {
                    {127,127,127}, {255,0,0}, {0,255,0}, {255,255,0},
                    {92,92,255}, {255,0,255}, {0,255,255}, {255,255,255}
                };
                fg = table[code - 90];
            }
        }
    }

    static int get_char_width(uint32_t cp) {
        if (cp < 0x1100) return 1;
        if ((cp >= 0x1100 && cp <= 0x115f) ||
            (cp >= 0x2329 && cp <= 0x232a) ||
            (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) ||
            (cp >= 0xac00 && cp <= 0xd7a3) ||
            (cp >= 0xf900 && cp <= 0xfaff) ||
            (cp >= 0xfe10 && cp <= 0xfe19) ||
            (cp >= 0xfe30 && cp <= 0xfe6f) ||
            (cp >= 0xff00 && cp <= 0xff60) ||
            (cp >= 0xffe0 && cp <= 0xffe6) ||
            (cp >= 0x1f000 && cp <= 0x1fbff)) {
            return 2;
        }
        return 1;
    }

    static bool is_grapheme_extend(uint32_t cp) {
        // Zero Width Joiner
        if (cp == 0x200D) return true;
        // Variation Selectors
        if (cp >= 0xFE00 && cp <= 0xFE0F) return true;
        if (cp >= 0xE0100 && cp <= 0xE01EF) return true;
        // Skin tones (Emoji Modifier)
        if (cp >= 0x1F3FB && cp <= 0x1F3FF) return true;
        // Combining Marks (Basic range)
        if (cp >= 0x0300 && cp <= 0x036F) return true;
        return false;
    }

    void draw_text(const std::string& text, float x, float y,
                   const std::string&, float, bool, const Color& color) override {
        float cur_x = x + tx_;
        float cur_y = y + ty_;
        
        // Quick clip check (approximate, since newlines can go down)
        // if (cur_y + CHAR_HEIGHT < clip_rect_.y) return; 
        // We can't prune strictly because of wrapping/newlines.

        int start_col = px_to_col(cur_x);
        int col = start_col;
        int row = px_to_row(cur_y);
        
        ::tui::Color current_fg = to_color(color);
        ::tui::Color base_fg = current_fg;
        ::tui::Color current_bg = ::tui::Color::black();
        ::tui::Style current_style = ::tui::Style::None;
        bool bg_set = false;

        const char* p = text.data();
        const char* end = p + text.length();
        int offset_x = 0;
        int row_offset = 0;

        while (p < end) {
            // ANSI handling...
            if (*p == '\x1b' && (p + 1 < end) && *(p + 1) == '[') {
                p += 2;
                parse_sgr(p, end, current_fg, current_bg, current_style, base_fg, bg_set);
                continue;
            }

            const char* cluster_start = p;
            uint32_t cp = decode_utf8(p, end);
            if (cp == 0) continue;

            if (cp == '\n') {
                row_offset++;
                offset_x = 0;
                continue;
            }

            // Check for grapheme cluster
            bool is_cluster = false;
            while (p < end) {
                const char* peek = p;
                uint32_t next_cp = decode_utf8(peek, end);
                if (is_grapheme_extend(next_cp)) {
                    is_cluster = true;
                    p = peek;
                } else {
                    break;
                }
            }
            
            uint32_t cluster_idx = 0;
            if (is_cluster) {
                std::string cluster_str(cluster_start, p - cluster_start);
                cluster_idx = buf_.add_cluster(cluster_str);
            }

            int width = get_char_width(cp);
            
            // Per-character clipping
            // Position relative to start (handling multiline)
            float cell_px_x = (col + offset_x) * CHAR_WIDTH + CHAR_WIDTH/2.0f;
            float cell_px_y = (row + row_offset) * CHAR_HEIGHT + CHAR_HEIGHT/2.0f;
            
            if (clip_rect_.contains(cell_px_x, cell_px_y)) {
                if (buf_.in_bounds(col + offset_x, row + row_offset)) {
                    // Preserve background if not set by ANSI
                    ::tui::Color eff_bg = bg_set ? current_bg : buf_.at(col + offset_x, row + row_offset).bg;
                    
                    ::tui::Cell cell{cp, current_fg, eff_bg, current_style};
                    cell.cluster_index = cluster_idx;
                    buf_.set(col + offset_x, row + row_offset, cell);
                    
                    if (width > 1 && buf_.in_bounds(col + offset_x + 1, row + row_offset)) {
                        // Wide char trailing cell
                         ::tui::Color eff_bg_trail = bg_set ? current_bg : buf_.at(col + offset_x + 1, row + row_offset).bg;
                        buf_.set(col + offset_x + 1, row + row_offset, ::tui::Cell{U'\0', current_fg, eff_bg_trail, current_style});
                    }
                }
            }
            
            offset_x += width;
        }
    }

    void draw_image(const std::string&, float, float, float, float) override {}
    void draw_svg(const std::string&, float, float, float, float) override {}
    void draw_svg_data(const std::string&, float, float, float, float) override {}

    void clear(const Color& c) override { buf_.clear(to_color(c)); }

    Bounds viewport() const override { return {0, 0, frame_w_, frame_h_}; }

    // ========== Retained Mode (not supported) ==========
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
    ::tui::Terminal& term_;
    ::tui::Buffer& buf_;
    float frame_w_ = 0, frame_h_ = 0;
    float tx_ = 0, ty_ = 0;
    float alpha_ = 1.0f;
    
    turbo_arena_t arena_;
    SaveState* save_stack_data_ = nullptr;
    size_t save_stack_ptr_ = 0;
    size_t save_stack_capacity_ = 0;
};

// Factory function
inline std::unique_ptr<Renderer> create_renderer(::tui::Terminal& term) {
    return std::make_unique<TuiRenderer>(term);
}

} // namespace tui_backend
} // namespace flex
