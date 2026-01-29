/*
 * TUI - Terminal User Interface Library
 * 
 * Buffer: A 2D grid of cells representing terminal content.
 * Supports double-buffering for flicker-free rendering.
 */

#pragma once

#include "cell.h"
#include <vector>
#include <string>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace tui {

class Buffer {
public:
    Buffer() = default;
    Buffer(int width, int height) : width_(width), height_(height), cells_(width * height) {
        reset_dirty();
    }
    
    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        cells_.resize(width * height);
        clear();
        reset_dirty();
    }
    
    int width() const { return width_; }
    int height() const { return height_; }
    
    // Direct cell access
    Cell& at(int x, int y) { return cells_[y * width_ + x]; }
    const Cell& at(int x, int y) const { return cells_[y * width_ + x]; }
    
    Cell& operator()(int x, int y) { return at(x, y); }
    const Cell& operator()(int x, int y) const { return at(x, y); }
    
    // Bounds checking
    bool in_bounds(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }
    
    // Clear buffer
    void mark_all_dirty() {
        if (width_ > 0 && height_ > 0) {
            dirty_min_x_ = 0;
            dirty_min_y_ = 0;
            dirty_max_x_ = width_ - 1;
            dirty_max_y_ = height_ - 1;
        }
    }

    void clear() { 
        std::fill(cells_.begin(), cells_.end(), Cell{}); 
        clear_clusters();
        mark_all_dirty();
    }
    void clear(const Cell& c) { 
        std::fill(cells_.begin(), cells_.end(), c); 
        clear_clusters();
        mark_all_dirty();
    }
    void clear(Color bg) { clear(Cell{U' ', Color::white(), bg}); }
    
    // Set a single cell (with bounds check)
    void set(int x, int y, const Cell& c) {
        if (in_bounds(x, y)) {
            at(x, y) = c;
            mark_dirty(x, y);
        }
    }
    
    void set(int x, int y, char32_t ch) {
        if (in_bounds(x, y)) {
            at(x, y).ch = ch;
            mark_dirty(x, y);
        }
    }
    
    // Draw text (ASCII)
    void text(int x, int y, const char* str, Color fg = Color::white(), Color bg = Color::black()) {
        int start_x = x;
        while (*str && x < width_) {
            if (in_bounds(x, y)) {
                at(x, y) = Cell{static_cast<char32_t>(*str), fg, bg};
            }
            ++str;
            ++x;
        }
        if (x > start_x) {
            mark_dirty(start_x, y);
            mark_dirty(x - 1, y);
        }
    }
    
    void text(int x, int y, const std::string& str, Color fg = Color::white(), Color bg = Color::black()) {
        text(x, y, str.c_str(), fg, bg);
    }
    
    // Draw horizontal line
    void hline(int x, int y, int len, char32_t ch = U'─', Color fg = Color::white(), Color bg = Color::black()) {
        for (int i = 0; i < len && x + i < width_; ++i) {
            set(x + i, y, Cell{ch, fg, bg});
        }
    }
    
    // Draw vertical line
    void vline(int x, int y, int len, char32_t ch = U'│', Color fg = Color::white(), Color bg = Color::black()) {
        for (int i = 0; i < len && y + i < height_; ++i) {
            set(x, y + i, Cell{ch, fg, bg});
        }
    }
    
    // Fill rectangle
    void fill(int x, int y, int w, int h, const Cell& c) {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                if (in_bounds(x + i, y + j)) {
                     at(x + i, y + j) = c;
                }
            }
        }
        mark_dirty(x, y);
        mark_dirty(x + w - 1, y + h - 1);
    }
    
    void fill(int x, int y, int w, int h, Color bg) {
        fill(x, y, w, h, Cell{U' ', Color::white(), bg});
    }
    
    // Draw box (single line)
    void box(int x, int y, int w, int h, Color fg = Color::white(), Color bg = Color::black()) {
        if (w < 2 || h < 2) return;
        
        // Corners
        set(x, y, Cell{U'┌', fg, bg});
        set(x + w - 1, y, Cell{U'┐', fg, bg});
        set(x, y + h - 1, Cell{U'└', fg, bg});
        set(x + w - 1, y + h - 1, Cell{U'┘', fg, bg});
        
        // Edges
        hline(x + 1, y, w - 2, U'─', fg, bg);
        hline(x + 1, y + h - 1, w - 2, U'─', fg, bg);
        vline(x, y + 1, h - 2, U'│', fg, bg);
        vline(x + w - 1, y + 1, h - 2, U'│', fg, bg);
    }
    
    // Draw double-line box
    void box_double(int x, int y, int w, int h, Color fg = Color::white(), Color bg = Color::black()) {
        if (w < 2 || h < 2) return;
        
        set(x, y, Cell{U'╔', fg, bg});
        set(x + w - 1, y, Cell{U'╗', fg, bg});
        set(x, y + h - 1, Cell{U'╚', fg, bg});
        set(x + w - 1, y + h - 1, Cell{U'╝', fg, bg});
        
        hline(x + 1, y, w - 2, U'═', fg, bg);
        hline(x + 1, y + h - 1, w - 2, U'═', fg, bg);
        vline(x, y + 1, h - 2, U'║', fg, bg);
        vline(x + w - 1, y + 1, h - 2, U'║', fg, bg);
    }
    
    // Draw rounded box
    void box_round(int x, int y, int w, int h, Color fg = Color::white(), Color bg = Color::black()) {
        if (w < 2 || h < 2) return;
        
        set(x, y, Cell{U'╭', fg, bg});
        set(x + w - 1, y, Cell{U'╮', fg, bg});
        set(x, y + h - 1, Cell{U'╰', fg, bg});
        set(x + w - 1, y + h - 1, Cell{U'╯', fg, bg});
        
        hline(x + 1, y, w - 2, U'─', fg, bg);
        hline(x + 1, y + h - 1, w - 2, U'─', fg, bg);
        vline(x, y + 1, h - 2, U'│', fg, bg);
        vline(x + w - 1, y + 1, h - 2, U'│', fg, bg);
    }
    
    // Draw heavy box (thick lines)
    void box_heavy(int x, int y, int w, int h, Color fg = Color::white(), Color bg = Color::black()) {
        if (w < 2 || h < 2) return;
        
        set(x, y, Cell{U'┏', fg, bg});
        set(x + w - 1, y, Cell{U'┓', fg, bg});
        set(x, y + h - 1, Cell{U'┗', fg, bg});
        set(x + w - 1, y + h - 1, Cell{U'┛', fg, bg});
        
        hline(x + 1, y, w - 2, U'━', fg, bg);
        hline(x + 1, y + h - 1, w - 2, U'━', fg, bg);
        vline(x, y + 1, h - 2, U'┃', fg, bg);
        vline(x + w - 1, y + 1, h - 2, U'┃', fg, bg);
    }
    
    // Fill with gradient (vertical, using block characters)
    void gradient_v(int x, int y, int w, int h, Color top, Color bottom) {
        for (int j = 0; j < h; ++j) {
            float t = static_cast<float>(j) / std::max(1, h - 1);
            Color c{
                static_cast<uint8_t>(top.r + t * (bottom.r - top.r)),
                static_cast<uint8_t>(top.g + t * (bottom.g - top.g)),
                static_cast<uint8_t>(top.b + t * (bottom.b - top.b))
            };
            for (int i = 0; i < w; ++i) {
                set(x + i, y + j, Cell{U'█', c, c});
            }
        }
    }
    
    // Fill with gradient (horizontal)
    void gradient_h(int x, int y, int w, int h, Color left, Color right) {
        for (int i = 0; i < w; ++i) {
            float t = static_cast<float>(i) / std::max(1, w - 1);
            Color c{
                static_cast<uint8_t>(left.r + t * (right.r - left.r)),
                static_cast<uint8_t>(left.g + t * (right.g - left.g)),
                static_cast<uint8_t>(left.b + t * (right.b - left.b))
            };
            for (int j = 0; j < h; ++j) {
                set(x + i, y + j, Cell{U'█', c, c});
            }
        }
    }
    
    // Draw shade pattern (light/medium/dark)
    void shade(int x, int y, int w, int h, int level, Color fg = Color::white(), Color bg = Color::black()) {
        char32_t ch = (level <= 0) ? U' ' : (level == 1) ? U'░' : (level == 2) ? U'▒' : U'▓';
        fill(x, y, w, h, Cell{ch, fg, bg});
    }
    
    // Draw progress bar using block characters (smoother than widgets version)
    void bar_h(int x, int y, int w, float progress, Color fg = Color::white(), Color bg = Color::black()) {
        progress = std::clamp(progress, 0.0f, 1.0f);
        float filled = progress * w;
        int full_blocks = static_cast<int>(filled);
        float frac = filled - full_blocks;
        
        // Full blocks
        for (int i = 0; i < full_blocks && i < w; ++i) {
            set(x + i, y, Cell{U'█', fg, bg});
        }
        
        // Partial block (using eighths: ▏▎▍▌▋▊▉█)
        if (full_blocks < w && frac > 0.0f) {
            static const char32_t eighths[] = {U' ', U'▏', U'▎', U'▍', U'▌', U'▋', U'▊', U'▉'};
            int idx = static_cast<int>(frac * 8);
            set(x + full_blocks, y, Cell{eighths[idx], fg, bg});
            full_blocks++;
        }
        
        // Empty space
        for (int i = full_blocks; i < w; ++i) {
            set(x + i, y, Cell{U' ', fg, bg});
        }
    }
    
    // Draw vertical bar
    void bar_v(int x, int y, int h, float progress, Color fg = Color::white(), Color bg = Color::black()) {
        progress = std::clamp(progress, 0.0f, 1.0f);
        float filled = progress * h;
        int full_blocks = static_cast<int>(filled);
        float frac = filled - full_blocks;
        
        // Empty space (top)
        for (int i = 0; i < h - full_blocks - 1; ++i) {
            set(x, y + i, Cell{U' ', fg, bg});
        }
        
        // Partial block (using eighths: ▁▂▃▄▅▆▇█)
        if (full_blocks < h && frac > 0.0f) {
            static const char32_t eighths[] = {U' ', U'▁', U'▂', U'▃', U'▄', U'▅', U'▆', U'▇'};
            int idx = static_cast<int>(frac * 8);
            set(x, y + h - full_blocks - 1, Cell{eighths[idx], fg, bg});
        }
        
        // Full blocks (bottom)
        for (int i = 0; i < full_blocks && i < h; ++i) {
            set(x, y + h - 1 - i, Cell{U'█', fg, bg});
        }
    }
    
    // Draw sparkline (mini chart)
    void sparkline(int x, int y, const float* values, int count, Color fg = Color::white(), Color bg = Color::black()) {
        static const char32_t bars[] = {U'▁', U'▂', U'▃', U'▄', U'▅', U'▆', U'▇', U'█'};
        
        float min_v = values[0], max_v = values[0];
        for (int i = 1; i < count; ++i) {
            min_v = std::min(min_v, values[i]);
            max_v = std::max(max_v, values[i]);
        }
        
        float range = max_v - min_v;
        if (range < 0.0001f) range = 1.0f;
        
        for (int i = 0; i < count; ++i) {
            float norm = (values[i] - min_v) / range;
            int idx = static_cast<int>(norm * 7.99f);
            set(x + i, y, Cell{bars[idx], fg, bg});
        }
    }
    
    // Raw cell data access
    const std::vector<Cell>& cells() const { return cells_; }
    std::vector<Cell>& cells() { return cells_; }

    // Grapheme cluster management
    uint32_t add_cluster(const std::string& cluster) {
        // 0 is reserved for "no cluster"
        if (clusters_.empty()) clusters_.push_back(""); 
        clusters_.push_back(cluster);
        return static_cast<uint32_t>(clusters_.size() - 1);
    }

    const std::string& get_cluster(uint32_t index) const {
        if (index > 0 && index < clusters_.size()) return clusters_[index];
        static const std::string empty;
        return empty;
    }

    // Clear clusters when clearing buffer
    void clear_clusters() {
        clusters_.clear();
        clusters_.push_back(""); // Reserve 0
    }

    // Dirty Rect Tracking
    void reset_dirty() {
        dirty_min_x_ = width_;
        dirty_min_y_ = height_;
        dirty_max_x_ = -1;
        dirty_max_y_ = -1;
    }

    void mark_dirty(int x, int y) {
        if (!in_bounds(x, y)) return;
        dirty_min_x_ = std::min(dirty_min_x_, x);
        dirty_min_y_ = std::min(dirty_min_y_, y);
        dirty_max_x_ = std::max(dirty_max_x_, x);
        dirty_max_y_ = std::max(dirty_max_y_, y);
    }
    
    // Check if buffer has any changes
    bool is_dirty() const {
        return dirty_max_x_ >= dirty_min_x_ && dirty_max_y_ >= dirty_min_y_;
    }

    // Get dirty bounds (inclusive)
    void get_dirty_bounds(int& min_x, int& min_y, int& max_x, int& max_y) const {
        min_x = dirty_min_x_;
        min_y = dirty_min_y_;
        max_x = dirty_max_x_;
        max_y = dirty_max_y_;
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Cell> cells_;
    std::vector<std::string> clusters_;
    
    // Dirty bounds
    int dirty_min_x_ = 0;
    int dirty_min_y_ = 0;
    int dirty_max_x_ = -1;
    int dirty_max_y_ = -1;
};

} // namespace tui
