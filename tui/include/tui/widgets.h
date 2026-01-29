/*
 * TUI - Terminal User Interface Library
 * 
 * Widgets: Common UI components drawn with characters.
 */

#pragma once

#include "buffer.h"
#include <string>
#include <algorithm>

// Prevent Windows min/max macros from breaking std::min/std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace tui {
namespace widgets {

// Theme colors
struct Theme {
    Color bg{30, 30, 30};
    Color fg{220, 220, 220};
    Color primary{100, 149, 237};    // Cornflower blue
    Color secondary{128, 128, 128};
    Color success{50, 205, 50};
    Color warning{255, 165, 0};
    Color error{220, 20, 60};
    Color border{80, 80, 80};
    Color highlight{70, 130, 180};
};

inline Theme default_theme() { return Theme{}; }

// Progress bar
inline void progress_bar(Buffer& buf, int x, int y, int width, float progress, 
                         const Theme& theme = default_theme()) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    int filled = static_cast<int>(progress * (width - 2));
    
    // Border
    buf.set(x, y, Cell{U'[', theme.fg, theme.bg});
    buf.set(x + width - 1, y, Cell{U']', theme.fg, theme.bg});
    
    // Fill
    for (int i = 1; i < width - 1; ++i) {
        char32_t ch = (i - 1 < filled) ? U'█' : U'░';
        Color fg = (i - 1 < filled) ? theme.primary : theme.secondary;
        buf.set(x + i, y, Cell{ch, fg, theme.bg});
    }
}

// Progress bar with label
inline void progress_bar(Buffer& buf, int x, int y, int width, float progress,
                         const std::string& label, const Theme& theme = default_theme()) {
    progress_bar(buf, x, y, width, progress, theme);
    
    // Center label
    int label_x = x + (width - static_cast<int>(label.size())) / 2;
    buf.text(label_x, y, label, theme.fg, theme.bg);
}

// Button
inline void button(Buffer& buf, int x, int y, int width, const std::string& label,
                   bool focused = false, bool pressed = false, const Theme& theme = default_theme()) {
    Color bg = pressed ? theme.highlight : (focused ? theme.primary : theme.secondary);
    Color fg = theme.fg;
    
    // Draw button background
    for (int i = 0; i < width; ++i) {
        buf.set(x + i, y, Cell{U' ', fg, bg});
    }
    
    // Center label
    int label_x = x + (width - static_cast<int>(label.size())) / 2;
    buf.text(label_x, y, label, fg, bg);
}

// Checkbox
inline void checkbox(Buffer& buf, int x, int y, bool checked, const std::string& label,
                     const Theme& theme = default_theme()) {
    char32_t box = checked ? U'☑' : U'☐';
    buf.set(x, y, Cell{box, theme.primary, theme.bg});
    buf.text(x + 2, y, label, theme.fg, theme.bg);
}

// Radio button
inline void radio(Buffer& buf, int x, int y, bool selected, const std::string& label,
                  const Theme& theme = default_theme()) {
    char32_t dot = selected ? U'◉' : U'○';
    buf.set(x, y, Cell{dot, theme.primary, theme.bg});
    buf.text(x + 2, y, label, theme.fg, theme.bg);
}

// Toggle switch
inline void toggle(Buffer& buf, int x, int y, bool on, const Theme& theme = default_theme()) {
    Color bg = on ? theme.success : theme.secondary;
    
    // Track (4 chars wide)
    buf.set(x, y, Cell{U'(', theme.fg, theme.bg});
    buf.set(x + 1, y, Cell{on ? U'●' : U' ', theme.fg, bg});
    buf.set(x + 2, y, Cell{on ? U' ' : U'●', theme.fg, bg});
    buf.set(x + 3, y, Cell{U')', theme.fg, theme.bg});
}

// Slider
inline void slider(Buffer& buf, int x, int y, int width, float value,
                   const Theme& theme = default_theme()) {
    value = std::clamp(value, 0.0f, 1.0f);
    int pos = static_cast<int>(value * (width - 1));
    
    for (int i = 0; i < width; ++i) {
        char32_t ch = (i == pos) ? U'●' : U'─';
        Color fg = (i <= pos) ? theme.primary : theme.secondary;
        buf.set(x + i, y, Cell{ch, fg, theme.bg});
    }
}

// Spinner (animated)
inline void spinner(Buffer& buf, int x, int y, int frame, const Theme& theme = default_theme()) {
    static const char32_t frames[] = {U'⠋', U'⠙', U'⠹', U'⠸', U'⠼', U'⠴', U'⠦', U'⠧', U'⠇', U'⠏'};
    char32_t ch = frames[frame % 10];
    buf.set(x, y, Cell{ch, theme.primary, theme.bg});
}

// Horizontal divider
inline void divider(Buffer& buf, int x, int y, int width, const Theme& theme = default_theme()) {
    for (int i = 0; i < width; ++i) {
        buf.set(x + i, y, Cell{U'─', theme.border, theme.bg});
    }
}

// Panel with title
inline void panel(Buffer& buf, int x, int y, int width, int height, 
                  const std::string& title = "", const Theme& theme = default_theme()) {
    // Fill background
    buf.fill(x, y, width, height, theme.bg);
    
    // Border
    buf.box_round(x, y, width, height, theme.border, theme.bg);
    
    // Title
    if (!title.empty()) {
        int title_x = x + 2;
        buf.set(title_x - 1, y, Cell{U' ', theme.fg, theme.bg});
        buf.text(title_x, y, title, theme.fg, theme.bg);
        buf.set(title_x + static_cast<int>(title.size()), y, Cell{U' ', theme.fg, theme.bg});
    }
}

// Text input field
inline void input(Buffer& buf, int x, int y, int width, const std::string& text,
                  int cursor = -1, bool focused = false, const Theme& theme = default_theme()) {
    Color bg = focused ? Color{50, 50, 50} : theme.bg;
    
    // Background
    for (int i = 0; i < width; ++i) {
        buf.set(x + i, y, Cell{U' ', theme.fg, bg});
    }
    
    // Text
    int text_len = std::min(static_cast<int>(text.size()), width - 2);
    buf.text(x + 1, y, text.substr(0, text_len), theme.fg, bg);
    
    // Cursor
    if (focused && cursor >= 0 && cursor <= text_len) {
        buf.set(x + 1 + cursor, y, Cell{U'▏', theme.primary, bg});
    }
    
    // Border
    buf.set(x, y, Cell{U'│', theme.border, theme.bg});
    buf.set(x + width - 1, y, Cell{U'│', theme.border, theme.bg});
}

// Badge/tag
inline void badge(Buffer& buf, int x, int y, const std::string& text, 
                  Color bg_color, const Theme& theme = default_theme()) {
    int width = static_cast<int>(text.size()) + 2;
    
    buf.set(x, y, Cell{U' ', theme.fg, bg_color});
    buf.text(x + 1, y, text, Color::white(), bg_color);
    buf.set(x + width - 1, y, Cell{U' ', theme.fg, bg_color});
}

// Scrollbar (vertical)
inline void scrollbar_v(Buffer& buf, int x, int y, int height, float position, float visible_ratio,
                        const Theme& theme = default_theme()) {
    position = std::clamp(position, 0.0f, 1.0f);
    visible_ratio = std::clamp(visible_ratio, 0.0f, 1.0f);
    
    int thumb_height = std::max(1, static_cast<int>(height * visible_ratio));
    int thumb_pos = static_cast<int>(position * (height - thumb_height));
    
    for (int i = 0; i < height; ++i) {
        bool is_thumb = (i >= thumb_pos && i < thumb_pos + thumb_height);
        char32_t ch = is_thumb ? U'█' : U'░';
        Color fg = is_thumb ? theme.primary : theme.secondary;
        buf.set(x, y + i, Cell{ch, fg, theme.bg});
    }
}

// Table header
inline void table_header(Buffer& buf, int x, int y, const std::vector<std::string>& columns,
                         const std::vector<int>& widths, const Theme& theme = default_theme()) {
    int cx = x;
    for (size_t i = 0; i < columns.size(); ++i) {
        int w = (i < widths.size()) ? widths[i] : 10;
        
        // Header cell
        for (int j = 0; j < w; ++j) {
            buf.set(cx + j, y, Cell{U' ', theme.fg, theme.highlight});
        }
        buf.text(cx + 1, y, columns[i].substr(0, w - 2), theme.fg, theme.highlight);
        
        cx += w;
    }
}

// Table row
inline void table_row(Buffer& buf, int x, int y, const std::vector<std::string>& cells,
                      const std::vector<int>& widths, bool selected = false,
                      const Theme& theme = default_theme()) {
    Color bg = selected ? theme.highlight : theme.bg;
    int cx = x;
    
    for (size_t i = 0; i < cells.size(); ++i) {
        int w = (i < widths.size()) ? widths[i] : 10;
        
        for (int j = 0; j < w; ++j) {
            buf.set(cx + j, y, Cell{U' ', theme.fg, bg});
        }
        buf.text(cx + 1, y, cells[i].substr(0, w - 2), theme.fg, bg);
        
        cx += w;
    }
}

} // namespace widgets
} // namespace tui
