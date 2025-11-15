#pragma once

#include <nanogui/widget.h>
#include <nanovg.h>
#include <string>
#include <functional>

namespace whiteboard {

class WhiteboardDocument;
class ExportManager;

class ExportDialog : public nanogui::Widget {
public:
    enum class Format {
        PNG,
        SVG,
        PDF
    };
    
    enum class Scope {
        EntireCanvas,
        VisibleArea,
        SelectedShapes
    };
    
    ExportDialog(nanogui::Widget* parent, WhiteboardDocument* document);
    
    void show();
    void hide();
    bool is_visible() const { return m_is_visible; }
    
    void set_export_callback(std::function<void(Format, Scope, int)> callback) {
        m_export_callback = callback;
    }
    
    nanogui::Vector2i preferred_size_impl(NVGcontext*) const override;
    bool mouse_button_event(const nanogui::Vector2i& p, int button, bool down, int modifiers) override;
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;
    void draw(NVGcontext* ctx) override;
    
private:
    WhiteboardDocument* m_document;
    Format m_format;
    Scope m_scope;
    int m_png_quality;  // 1x, 2x, 4x
    bool m_is_visible;
    int m_hovered_button;  // -1 = none, 0 = export, 1 = cancel
    
    std::function<void(Format, Scope, int)> m_export_callback;
    
    void draw_background(NVGcontext* ctx);
    void draw_panel(NVGcontext* ctx);
    void draw_header(NVGcontext* ctx, float& y);
    void draw_format_selector(NVGcontext* ctx, float& y);
    void draw_scope_selector(NVGcontext* ctx, float& y);
    void draw_quality_selector(NVGcontext* ctx, float& y);
    void draw_buttons(NVGcontext* ctx, float& y);
    
    void on_export_clicked();
    void on_cancel_clicked();
};

} // namespace whiteboard
