#include "whiteboard/ui/export_dialog.h"
#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/opengl.h>
#include <nanogui/theme.h>
#include <nanogui/keys.h>
#include <nanovg.h>

namespace whiteboard {

ExportDialog::ExportDialog(nanogui::Widget* parent, WhiteboardDocument* document)
    : Widget(parent), m_document(document), m_format(Format::PNG), 
      m_scope(Scope::EntireCanvas), m_png_quality(2), m_is_visible(false),
      m_hovered_button(-1) {
    set_visible(false);
}

void ExportDialog::show() {
    m_is_visible = true;
    set_visible(true);
}

void ExportDialog::hide() {
    m_is_visible = false;
    set_visible(false);
}

nanogui::Vector2i ExportDialog::preferred_size_impl(NVGcontext*) const {
    return nanogui::Vector2i(500, 450);
}

bool ExportDialog::mouse_button_event(const nanogui::Vector2i& p, int button, bool down, int modifiers) {
    if (!m_is_visible)
        return false;
    
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 500;
    int panel_height = 450;
    int panel_x = (root->width() - panel_width) / 2;
    int panel_y = (root->height() - panel_height) / 2;
    
    // Check if clicking outside the panel
    if (p.x() < panel_x || p.x() > panel_x + panel_width ||
        p.y() < panel_y || p.y() > panel_y + panel_height) {
        if (down) {
            hide();
        }
        return true;
    }
    
    // Check button clicks
    if (down && button == 0) {
        float btn_y = panel_y + 380;
        float export_btn_x = panel_x + panel_width / 2 - 110;
        float cancel_btn_x = panel_x + panel_width / 2 + 10;
        
        if (p.y() >= btn_y && p.y() <= btn_y + 40) {
            if (p.x() >= export_btn_x && p.x() <= export_btn_x + 100) {
                on_export_clicked();
                return true;
            }
            if (p.x() >= cancel_btn_x && p.x() <= cancel_btn_x + 100) {
                on_cancel_clicked();
                return true;
            }
        }
        
        // Check format buttons
        float format_y = panel_y + 100;
        float format_x = panel_x + 50;
        for (int i = 0; i < 3; ++i) {
            float btn_x = format_x + i * 120;
            if (p.x() >= btn_x && p.x() <= btn_x + 100 &&
                p.y() >= format_y && p.y() <= format_y + 40) {
                m_format = static_cast<Format>(i);
                return true;
            }
        }
        
        // Check scope buttons
        float scope_y = panel_y + 200;
        for (int i = 0; i < 3; ++i) {
            float btn_x = format_x + i * 120;
            if (p.x() >= btn_x && p.x() <= btn_x + 100 &&
                p.y() >= scope_y && p.y() <= scope_y + 40) {
                m_scope = static_cast<Scope>(i);
                return true;
            }
        }
        
        // Check quality buttons (only for PNG)
        if (m_format == Format::PNG) {
            float quality_y = panel_y + 300;
            for (int i = 0; i < 3; ++i) {
                float btn_x = format_x + i * 80;
                if (p.x() >= btn_x && p.x() <= btn_x + 60 &&
                    p.y() >= quality_y && p.y() <= quality_y + 40) {
                    m_png_quality = (i == 0) ? 1 : (i == 1) ? 2 : 4;
                    return true;
                }
            }
        }
    }
    
    return Widget::mouse_button_event(p, button, down, modifiers);
}

bool ExportDialog::keyboard_event(int key, int scancode, int action, int modifiers) {
    if (!m_is_visible)
        return false;
    
    if (action == NANOGUI_KEY_PRESS) {
        if (key == NANOGUI_KEY_ESCAPE) {
            hide();
            return true;
        }
        if (key == NANOGUI_KEY_ENTER) {
            on_export_clicked();
            return true;
        }
    }
    
    return Widget::keyboard_event(key, scancode, action, modifiers);
}

void ExportDialog::draw(NVGcontext* ctx) {
    if (!m_is_visible)
        return;
    
    draw_background(ctx);
    draw_panel(ctx);
    
    Widget::draw(ctx);
}

void ExportDialog::draw_background(NVGcontext* ctx) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, (float)root->width(), (float)root->height());
    nvgFillColor(ctx, nvgRGBA(0, 0, 0, 128));
    nvgFill(ctx);
}

void ExportDialog::draw_panel(NVGcontext* ctx) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 500;
    int panel_height = 450;
    int panel_x = (root->width() - panel_width) / 2;
    int panel_y = (root->height() - panel_height) / 2;
    
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    nvgSave(ctx);
    
    // Draw shadow
    NVGpaint shadow_paint = nvgBoxGradient(ctx, panel_x, panel_y + 4, panel_width, panel_height,
                                          12, 20, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, 14);
    nvgFillPaint(ctx, shadow_paint);
    nvgFill(ctx);
    
    // Draw panel background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, panel_x, panel_y, panel_width, panel_height, 12);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(43, 43, 43, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(250, 250, 250, 255));
    }
    nvgFill(ctx);
    
    // Draw border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, panel_x, panel_y, panel_width, panel_height, 12);
    if (dark_mode) {
        nvgStrokeColor(ctx, nvgRGBA(80, 80, 80, 255));
    } else {
        nvgStrokeColor(ctx, nvgRGBA(200, 200, 200, 255));
    }
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);
    
    float y = panel_y + 30;
    draw_header(ctx, y);
    draw_format_selector(ctx, y);
    draw_scope_selector(ctx, y);
    draw_quality_selector(ctx, y);
    draw_buttons(ctx, y);
    
    nvgRestore(ctx);
}

void ExportDialog::draw_header(NVGcontext* ctx, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 500;
    int panel_x = (root->width() - panel_width) / 2;
    
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    nvgFontSize(ctx, 24.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(224, 224, 224, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(50, 50, 50, 255));
    }
    nvgText(ctx, panel_x + 30, y, "Export Whiteboard", nullptr);
    
    y += 50;
}

void ExportDialog::draw_format_selector(NVGcontext* ctx, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_x = (root->width() - 500) / 2;
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    // Label
    nvgFontSize(ctx, 16.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(200, 200, 200, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
    }
    nvgText(ctx, panel_x + 30, y, "Format:", nullptr);
    
    y += 30;
    
    // Format buttons
    const char* formats[] = {"PNG", "SVG", "PDF"};
    for (int i = 0; i < 3; ++i) {
        float btn_x = panel_x + 50 + i * 120;
        bool selected = (static_cast<int>(m_format) == i);
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, btn_x, y, 100, 40, 6);
        if (selected) {
            nvgFillColor(ctx, nvgRGBA(33, 150, 243, 255));
        } else if (dark_mode) {
            nvgFillColor(ctx, nvgRGBA(60, 60, 60, 255));
        } else {
            nvgFillColor(ctx, nvgRGBA(230, 230, 230, 255));
        }
        nvgFill(ctx);
        
        nvgFontSize(ctx, 14.0f);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        if (selected || dark_mode) {
            nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        } else {
            nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
        }
        nvgText(ctx, btn_x + 50, y + 20, formats[i], nullptr);
    }
    
    y += 60;
}

void ExportDialog::draw_scope_selector(NVGcontext* ctx, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_x = (root->width() - 500) / 2;
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    // Label
    nvgFontSize(ctx, 16.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(200, 200, 200, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
    }
    nvgText(ctx, panel_x + 30, y, "Scope:", nullptr);
    
    y += 30;
    
    // Scope buttons
    const char* scopes[] = {"All", "Visible", "Selected"};
    for (int i = 0; i < 3; ++i) {
        float btn_x = panel_x + 50 + i * 120;
        bool selected = (static_cast<int>(m_scope) == i);
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, btn_x, y, 100, 40, 6);
        if (selected) {
            nvgFillColor(ctx, nvgRGBA(33, 150, 243, 255));
        } else if (dark_mode) {
            nvgFillColor(ctx, nvgRGBA(60, 60, 60, 255));
        } else {
            nvgFillColor(ctx, nvgRGBA(230, 230, 230, 255));
        }
        nvgFill(ctx);
        
        nvgFontSize(ctx, 14.0f);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        if (selected || dark_mode) {
            nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        } else {
            nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
        }
        nvgText(ctx, btn_x + 50, y + 20, scopes[i], nullptr);
    }
    
    y += 60;
}

void ExportDialog::draw_quality_selector(NVGcontext* ctx, float& y) {
    if (m_format != Format::PNG) {
        return;  // Only show for PNG
    }
    
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_x = (root->width() - 500) / 2;
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    // Label
    nvgFontSize(ctx, 16.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(200, 200, 200, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
    }
    nvgText(ctx, panel_x + 30, y, "Quality:", nullptr);
    
    y += 30;
    
    // Quality buttons
    const char* qualities[] = {"1x", "2x", "4x"};
    int quality_values[] = {1, 2, 4};
    for (int i = 0; i < 3; ++i) {
        float btn_x = panel_x + 50 + i * 80;
        bool selected = (m_png_quality == quality_values[i]);
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, btn_x, y, 60, 40, 6);
        if (selected) {
            nvgFillColor(ctx, nvgRGBA(33, 150, 243, 255));
        } else if (dark_mode) {
            nvgFillColor(ctx, nvgRGBA(60, 60, 60, 255));
        } else {
            nvgFillColor(ctx, nvgRGBA(230, 230, 230, 255));
        }
        nvgFill(ctx);
        
        nvgFontSize(ctx, 14.0f);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        if (selected || dark_mode) {
            nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        } else {
            nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
        }
        nvgText(ctx, btn_x + 30, y + 20, qualities[i], nullptr);
    }
    
    y += 60;
}

void ExportDialog::draw_buttons(NVGcontext* ctx, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 500;
    int panel_x = (root->width() - panel_width) / 2;
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    y = panel_x + 380;  // Fixed position at bottom
    
    // Export button
    float export_btn_x = panel_x + panel_width / 2 - 110;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, export_btn_x, y, 100, 40, 6);
    nvgFillColor(ctx, nvgRGBA(76, 175, 80, 255));  // Green
    nvgFill(ctx);
    
    nvgFontSize(ctx, 16.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgText(ctx, export_btn_x + 50, y + 20, "Export", nullptr);
    
    // Cancel button
    float cancel_btn_x = panel_x + panel_width / 2 + 10;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, cancel_btn_x, y, 100, 40, 6);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(200, 200, 200, 255));
    }
    nvgFill(ctx);
    
    nvgFontSize(ctx, 16.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(224, 224, 224, 255));
    } else {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));
    }
    nvgText(ctx, cancel_btn_x + 50, y + 20, "Cancel", nullptr);
}

void ExportDialog::on_export_clicked() {
    if (m_export_callback) {
        m_export_callback(m_format, m_scope, m_png_quality);
    }
    hide();
}

void ExportDialog::on_cancel_clicked() {
    hide();
}

} // namespace whiteboard
