/*
 * flexUI Designer - Code Preview Implementation
 */

#include "flexui_designer/code_preview.h"
#include <sstream>

namespace flexui_designer {

CodePreview::CodePreview() {
    width_ = 300;
    height_ = 400;
}

void CodePreview::render(flex::Renderer& renderer) {
    if (!visible_) return;
    
    render_background(renderer);
    
    // Title bar
    flex::Paint title_bg = flex::Paint::solid(flex::Color{0.16f, 0.16f, 0.18f, 1});
    renderer.draw_rect(x_, y_, width_, 32, 0, title_bg, flex::Paint::none(), 0);
    renderer.draw_text("Code Preview", x_ + 12, y_ + 21, "sans", 12, true, {1, 1, 1, 1});
    
    // Copy button
    float btn_x = x_ + width_ - 60, btn_y = y_ + 6;
    flex::Paint btn_bg = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 1});
    renderer.draw_rect(btn_x, btn_y, 50, 20, 3, btn_bg, flex::Paint::none(), 0);
    renderer.draw_text("Copy", btn_x + 12, btn_y + 14, "sans", 10, false, {1, 1, 1, 1});
    
    // Code area background
    float code_y = y_ + 32;
    float code_h = height_ - 32;
    flex::Paint code_bg = flex::Paint::solid(flex::Color{0.08f, 0.08f, 0.1f, 1});
    renderer.draw_rect(x_, code_y, width_, code_h, 0, code_bg, flex::Paint::none(), 0);
    
    // Parse and render code lines
    std::istringstream stream(code_);
    std::string line;
    int line_num = 1;
    float line_y = code_y + 16 - scroll_offset_;
    float line_h = 14;
    
    while (std::getline(stream, line)) {
        if (line_y >= code_y && line_y < y_ + height_ - 4) {
            render_line(renderer, line_y, line_num, line);
        }
        line_y += line_h;
        line_num++;
    }
    
    max_scroll_ = std::max(0.0f, (line_num * line_h) - code_h + 20);
    
    // Scrollbar
    if (max_scroll_ > 0) {
        float sb_h = code_h * (code_h / (line_num * line_h + 20));
        float sb_y = code_y + (scroll_offset_ / max_scroll_) * (code_h - sb_h);
        flex::Paint sb_color = flex::Paint::solid(flex::Color{0.4f, 0.4f, 0.45f, 0.5f});
        renderer.draw_rect(x_ + width_ - 6, sb_y, 4, sb_h, 2, sb_color, flex::Paint::none(), 0);
    }
}

void CodePreview::render_line(flex::Renderer& r, float y, int line_num, const std::string& text) {
    // Line number
    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%3d", line_num);
    r.draw_text(num_buf, x_ + 4, y, "sans", 10, false, {0.4f, 0.4f, 0.45f, 1});
    
    // Simple syntax highlighting
    flex::Color color{0.85f, 0.85f, 0.85f, 1};
    
    // Keywords
    if (text.find("auto") != std::string::npos || 
        text.find("void") != std::string::npos ||
        text.find("const") != std::string::npos ||
        text.find("return") != std::string::npos ||
        text.find("#include") != std::string::npos) {
        color = {0.6f, 0.6f, 1.0f, 1};  // Blue for keywords
    }
    // Comments
    else if (text.find("//") != std::string::npos || text.find("/*") != std::string::npos) {
        color = {0.5f, 0.6f, 0.5f, 1};  // Green for comments
    }
    // Strings
    else if (text.find('"') != std::string::npos) {
        color = {0.9f, 0.7f, 0.5f, 1};  // Orange for strings
    }
    
    r.draw_text(text, x_ + 36, y, "sans", 10, false, color);
}

bool CodePreview::handle_click(float x, float y) {
    // Check copy button
    float btn_x = x_ + width_ - 60, btn_y = y_ + 6;
    if (x >= btn_x && x < btn_x + 50 && y >= btn_y && y < btn_y + 20) {
        if (on_copy_) on_copy_();
        return true;
    }
    return false;
}

bool CodePreview::handle_scroll(float delta) {
    scroll_offset_ = std::max(0.0f, std::min(max_scroll_, scroll_offset_ - delta * 30));
    return true;
}

} // namespace flexui_designer
