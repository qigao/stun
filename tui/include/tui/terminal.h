/*
 * TUI - Terminal User Interface Library
 * 
 * Terminal: Low-level terminal control and rendering.
 * Handles escape sequences, raw mode, and differential updates.
 */

#pragma once

#include "buffer.h"
#include <string>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32 
#include <windows.h>
#include <io.h>
#define STDOUT_FILENO 1
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

namespace tui {

class Terminal {
public:
    Terminal() = default;
    ~Terminal() { cleanup(); }
    
    // Initialize terminal (raw mode, alternate screen, etc.)
    bool init() {
        if (initialized_) return true;
        
        // Auto-detect color mode
        const char* ct = std::getenv("COLORTERM");
        if (ct && (std::string(ct) == "truecolor" || std::string(ct) == "24bit")) {
            color_mode_ = ColorMode::TrueColor;
        } else {
            const char* term = std::getenv("TERM");
            if (term && std::string(term).find("256color") != std::string::npos) {
                color_mode_ = ColorMode::Ansi256;
            } else {
                // Fallback to 256 for basic xterm compatibility
                color_mode_ = ColorMode::Ansi256;
            }
        }
        
#ifdef _WIN32
        // Windows: enable virtual terminal processing
        SetConsoleOutputCP(CP_UTF8);
        h_out_ = GetStdHandle(STD_OUTPUT_HANDLE);
        h_in_ = GetStdHandle(STD_INPUT_HANDLE);
        
        GetConsoleMode(h_out_, &orig_out_mode_);
        GetConsoleMode(h_in_, &orig_in_mode_);
        
        DWORD out_mode = orig_out_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        DWORD in_mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
        
        SetConsoleMode(h_out_, out_mode);
        SetConsoleMode(h_in_, in_mode);
#else
        // Unix: raw mode
        tcgetattr(STDIN_FILENO, &orig_termios_);
        struct termios raw = orig_termios_;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= CS8;
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
        
        // Query terminal size
        query_size();
        
        // Setup buffers
        front_.resize(width_, height_);
        back_.resize(width_, height_);
        
        // Enter alternate screen, hide cursor
        write_raw("\033[?1049h");  // Alternate screen
        write_raw("\033[?25l");    // Hide cursor
        write_raw("\033[2J");      // Clear screen
        
        initialized_ = true;
        return true;
    }
    
    void cleanup() {
        if (!initialized_) return;
        
        // Show cursor, exit alternate screen
        write_raw("\033[?25h");
        write_raw("\033[?1049l");
        write_raw("\033[0m");
        flush();
        
#ifdef _WIN32
        SetConsoleMode(h_out_, orig_out_mode_);
        SetConsoleMode(h_in_, orig_in_mode_);
#else
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
#endif
        
        initialized_ = false;
    }
    
    // Get terminal dimensions
    int width() const { return width_; }
    int height() const { return height_; }
    
    // Query and update terminal size
    void query_size() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(h_out_, &csbi)) {
            width_ = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            height_ = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
#else
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            width_ = ws.ws_col;
            height_ = ws.ws_row;
        }
#endif
        if (width_ <= 0) width_ = 80;
        if (height_ <= 0) height_ = 24;
    }
    
    // Get back buffer for drawing
    Buffer& buffer() { return back_; }
    const Buffer& buffer() const { return back_; }
    
    // Render back buffer to terminal (differential update)
    void render() {
        // Resize if needed
        if (back_.width() != width_ || back_.height() != height_) {
            back_.resize(width_, height_);
            front_.resize(width_, height_);
            // Force full redraw
            front_.clear(Cell{U'\0', Color{}, Color{}});
            back_.mark_all_dirty();
        }
        
        output_.clear();
        output_.reserve(width_ * height_ * 20);  // Estimate
        
        Color last_fg{255, 255, 255};
        Color last_bg{0, 0, 0};
        Style last_style = Style::None;
        int last_x = -1000, last_y = -1000;
        
        // Dirty rect optimization
        int min_x = 0, min_y = 0, max_x = width_ - 1, max_y = height_ - 1;
        if (back_.is_dirty()) {
             back_.get_dirty_bounds(min_x, min_y, max_x, max_y);
        } else {
             // Nothing to do
             return; 
        }

        // Clamp to screen
        min_x = std::max(0, min_x);
        min_y = std::max(0, min_y);
        max_x = std::min(width_ - 1, max_x);
        max_y = std::min(height_ - 1, max_y);

        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const Cell& cell = back_(x, y);
                const Cell& prev = front_(x, y);
                
                // Skip unchanged cells
                if (cell == prev) continue;
                
                // Move cursor if not sequential
                if (last_x != x - 1 || last_y != y) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
                    output_ += buf;
                }
                
                // Update style if changed
                if (cell.style != last_style) {
                    output_ += "\033[0m";  // Reset
                    last_fg = Color{255, 255, 255};
                    last_bg = Color{0, 0, 0};
                    
                    if (has_style(cell.style, Style::Bold)) output_ += "\033[1m";
                    if (has_style(cell.style, Style::Dim)) output_ += "\033[2m";
                    if (has_style(cell.style, Style::Italic)) output_ += "\033[3m";
                    if (has_style(cell.style, Style::Underline)) output_ += "\033[4m";
                    if (has_style(cell.style, Style::Blink)) output_ += "\033[5m";
                    if (has_style(cell.style, Style::Reverse)) output_ += "\033[7m";
                    if (has_style(cell.style, Style::Strike)) output_ += "\033[9m";
                    
                    last_style = cell.style;
                }
                
                // Update colors if changed
                if (cell.fg != last_fg) {
                    char buf[32];
                    if (color_mode_ == ColorMode::TrueColor) {
                        std::snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", cell.fg.r, cell.fg.g, cell.fg.b);
                    } else {
                        std::snprintf(buf, sizeof(buf), "\033[38;5;%dm", quantize_256(cell.fg));
                    }
                    output_ += buf;
                    last_fg = cell.fg;
                }
                
                if (cell.bg != last_bg) {
                    char buf[32];
                    if (color_mode_ == ColorMode::TrueColor) {
                        std::snprintf(buf, sizeof(buf), "\033[48;2;%d;%d;%dm", cell.bg.r, cell.bg.g, cell.bg.b);
                    } else {
                        std::snprintf(buf, sizeof(buf), "\033[48;5;%dm", quantize_256(cell.bg));
                    }
                    output_ += buf;
                    last_bg = cell.bg;
                }
                
                // Output character (UTF-8 encode)
                if (cell.cluster_index > 0) {
                    output_ += back_.get_cluster(cell.cluster_index);
                } else if (cell.ch != U'\0') {
                    encode_utf8(cell.ch, output_);
                }
                
                last_x = x;
                last_y = y;
                
                // Update front buffer
                front_(x, y) = cell;
            }
        }
        
        // Write to terminal
        write_raw(output_);
        flush();
        
        back_.reset_dirty();
    }
    
    // Force full redraw
    void refresh() {
        front_.clear(Cell{U'\0', Color{}, Color{}});
        render();
    }
    
    // Clear screen
    void clear() {
        back_.clear();
    }
    
    void clear(Color bg) {
        back_.clear(bg);
    }
    
    // Move cursor
    void move_cursor(int x, int y) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
        write_raw(buf);
        flush();
    }
    
    // Show/hide cursor
    void show_cursor() { write_raw("\033[?25h"); flush(); }
    void hide_cursor() { write_raw("\033[?25l"); flush(); }

private:
    void write_raw(const std::string& s) {
#ifdef _WIN32
        DWORD written;
        WriteConsoleA(h_out_, s.c_str(), static_cast<DWORD>(s.size()), &written, nullptr);
#else
        ::write(STDOUT_FILENO, s.c_str(), s.size());
#endif
    }
    
    void write_raw(const char* s) {
        write_raw(std::string(s));
    }
    
    void flush() {
        std::fflush(stdout);
    }
    
    static void encode_utf8(char32_t cp, std::string& out) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // Color Quantization (Redmean approximation)
    int quantize_256(const Color& c) {
        // Check cache (simple linear scan for cache of 64 recent colors might be faster than map, 
        // but map is O(1) avg. Let's effectively use a map or just compute it. 
        // Computing is heavily math bound.
        
        // Find best match in xterm-256 palette
        int best_idx = 0;
        int min_dist = 0x7FFFFFFF;
        
        // 1. Grayscale ramp checks (232-255)
        // 232: 08,08,08 ... 255: ee,ee,ee (step 10)
        // If color is near gray, check these.
        
        // 2. 6x6x6 Cube (16-231)
        // r,g,b in [0, 95, 135, 175, 215, 255]
        
        // 3. System colors (0-15)
        
        // Optimized search:
        // Try to map to cube first
        int q_r = std::clamp(static_cast<int>(c.r), 0, 255);
        int q_g = std::clamp(static_cast<int>(c.g), 0, 255);
        int q_b = std::clamp(static_cast<int>(c.b), 0, 255);
        
        // Helper to get dist
        auto dist_sq = [&](int r, int g, int b) {
            int dr = q_r - r;
            int dg = q_g - g;
            int db = q_b - b;
            // Redmean
            int r_mean = (q_r + r) / 2;
            int w_r = 2 + (r_mean >> 8);
            int w_g = 4;
            int w_b = 2 + ((255 - r_mean) >> 8);
            return w_r * dr*dr + w_g * dg*dg + w_b * db*db;
        };

        // Try cube direct mapping
        auto snap = [](int x) {
            if (x < 48) return 0;
            if (x < 115) return 95;
            if (x < 155) return 135;
            if (x < 195) return 175;
            if (x < 235) return 215;
            return 255;
        };
        auto map_val = [](int x) {
             if (x == 0) return 0;
             return (x - 55) / 40;
        };

        int cr = snap(q_r);
        int cg = snap(q_g);
        int cb = snap(q_b);
        int idx = 16 + 36 * map_val(cr) + 6 * map_val(cg) + map_val(cb);
        
        best_idx = idx;
        min_dist = dist_sq(cr, cg, cb);

        // Try Grays
        if (std::abs(q_r - q_g) < 16 && std::abs(q_r - q_b) < 16) {
             // Approximation: (val - 8) / 10
             int gray_val = (q_r + q_g + q_b) / 3;
             if (gray_val >= 8 && gray_val <= 238) {
                  int gi = (gray_val - 8) / 10;
                  int gv = 8 + gi * 10;
                  int d = dist_sq(gv, gv, gv);
                  if (d < min_dist) {
                      min_dist = d;
                      best_idx = 232 + gi;
                  }
                  // Check neighbor
                  if (gi < 23) {
                      gv += 10;
                      d = dist_sq(gv, gv, gv);
                      if (d < min_dist) {
                          min_dist = d;
                          best_idx = 232 + gi + 1;
                      }
                  }
             }
        }
        
        // For strict "closest", we should iterate, but the direct mapping is usually sufficient for TUI
        // unless we want "palette mapping" for 16-color terminals.
        
        return best_idx;
    }
    
    bool initialized_ = false;
    int width_ = 80;
    int height_ = 24;

    enum class ColorMode { TrueColor, Ansi256 };
    ColorMode color_mode_ = ColorMode::TrueColor;
    
    Buffer front_;  // What's currently on screen
    Buffer back_;   // What we're drawing to
    std::string output_;  // Escape sequence buffer
    
#ifdef _WIN32
    HANDLE h_out_ = INVALID_HANDLE_VALUE;
    HANDLE h_in_ = INVALID_HANDLE_VALUE;
    DWORD orig_out_mode_ = 0;
    DWORD orig_in_mode_ = 0;
#else
    struct termios orig_termios_;
#endif
};

} // namespace tui

