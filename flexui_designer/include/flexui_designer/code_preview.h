/*
 * flexUI Designer - Code Preview Panel
 * 
 * Real-time code preview showing generated C++ code.
 */

#pragma once

#include <meta_editor/view/panel.h>
#include <string>
#include <functional>

namespace flexui_designer {

class CodePreview : public meta_editor::Panel {
public:
    CodePreview();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;

    void set_code(const std::string& code) { code_ = code; }
    const std::string& code() const { return code_; }
    
    void set_copy_callback(std::function<void()> cb) { on_copy_ = std::move(cb); }
    
    bool handle_scroll(float delta);

private:
    std::string code_;
    float scroll_offset_ = 0;
    float max_scroll_ = 0;
    std::function<void()> on_copy_;
    
    void render_line(flex::Renderer& r, float y, int line_num, const std::string& text);
};

} // namespace flexui_designer
