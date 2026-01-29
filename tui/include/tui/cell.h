/*
 * TUI - Terminal User Interface Library
 * 
 * Cell: The fundamental unit of terminal display.
 * One cell = one character position on screen.
 */

#pragma once

#include <cstdint>
#include <string>

namespace tui {

// 24-bit RGB color
struct Color {
    uint8_t r = 0, g = 0, b = 0;
    
    constexpr Color() = default;
    constexpr Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    constexpr Color(uint32_t rgb) : r((rgb >> 16) & 0xFF), g((rgb >> 8) & 0xFF), b(rgb & 0xFF) {}
    
    constexpr uint32_t to_u32() const { return (r << 16) | (g << 8) | b; }
    constexpr bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b; }
    constexpr bool operator!=(const Color& o) const { return !(*this == o); }
    
    // Common colors
    static constexpr Color black()   { return {0, 0, 0}; }
    static constexpr Color white()   { return {255, 255, 255}; }
    static constexpr Color red()     { return {255, 0, 0}; }
    static constexpr Color green()   { return {0, 255, 0}; }
    static constexpr Color blue()    { return {0, 0, 255}; }
    static constexpr Color yellow()  { return {255, 255, 0}; }
    static constexpr Color cyan()    { return {0, 255, 255}; }
    static constexpr Color magenta() { return {255, 0, 255}; }
    static constexpr Color gray()    { return {128, 128, 128}; }
};

// Text style flags
enum class Style : uint8_t {
    None      = 0,
    Bold      = 1 << 0,
    Dim       = 1 << 1,
    Italic    = 1 << 2,
    Underline = 1 << 3,
    Blink     = 1 << 4,
    Reverse   = 1 << 5,
    Strike    = 1 << 6,
};

constexpr Style operator|(Style a, Style b) {
    return static_cast<Style>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr Style operator&(Style a, Style b) {
    return static_cast<Style>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

constexpr bool has_style(Style s, Style flag) {
    return (static_cast<uint8_t>(s) & static_cast<uint8_t>(flag)) != 0;
}

// A single terminal cell
struct Cell {
    char32_t ch = U' ';      // Unicode codepoint (supports emoji, CJK, etc.)
    Color fg = Color::white();
    Color bg = Color::black();
    Style style = Style::None;
    
    constexpr Cell() = default;
    constexpr Cell(char32_t c) : ch(c) {}
    constexpr Cell(char32_t c, Color fg, Color bg) : ch(c), fg(fg), bg(bg) {}
    constexpr Cell(char32_t c, Color fg, Color bg, Style s) : ch(c), fg(fg), bg(bg), style(s) {}
    
    constexpr bool operator==(const Cell& o) const {
        return ch == o.ch && fg == o.fg && bg == o.bg && style == o.style;
    }
    constexpr bool operator!=(const Cell& o) const { return !(*this == o); }
    
    // Set foreground color
    constexpr Cell& set_fg(Color c) { fg = c; return *this; }
    constexpr Cell& set_bg(Color c) { bg = c; return *this; }
    constexpr Cell& set_style(Style s) { style = s; return *this; }
    constexpr Cell& add_style(Style s) { style = style | s; return *this; }

    // Grapheme cluster support
    uint32_t cluster_index = 0; // 0 = standard single char (ch), >0 = index into Buffer::clusters
};

} // namespace tui
