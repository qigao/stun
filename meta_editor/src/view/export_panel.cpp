/*
 * Meta Editor - Export Panel Implementation
 */

#include "meta_editor/view/export_panel.h"
#include "meta_editor/core/editor.h"

namespace meta_editor {

ExportPanel::ExportPanel(Editor* editor) : editor_(editor) {
    set_size(280, 200);
}

void ExportPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);

    // Title
    renderer.draw_text("Export", x_ + 12, y_ + 24, "Arial", 14.0f, true, 
                       flex::Color{1.0f, 1.0f, 1.0f, 1.0f});

    float row_y = y_ + 50;
    float label_x = x_ + 12;
    float value_x = x_ + 100;

    // Format selection
    renderer.draw_text("Format:", label_x, row_y, "Arial", 12.0f, false,
                       flex::Color{0.8f, 0.8f, 0.8f, 1.0f});
    
    const char* formats[] = {"PNG", "SVG"};
    for (int i = 0; i < 2; ++i) {
        float bx = value_x + i * 60;
        bool selected = (selected_format_ == i);
        flex::Paint btn_bg = selected 
            ? flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.8f, 1.0f})
            : flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.25f, 1.0f});
        renderer.draw_rect(bx, row_y - 14, 50, 22, 4.0f, btn_bg, flex::Paint::none(), 0);
        renderer.draw_text(formats[i], bx + 15, row_y, "Arial", 11.0f, false,
                          flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
    }

    row_y += 35;

    // Scale (PNG only)
    if (selected_format_ == 0) {
        renderer.draw_text("Scale:", label_x, row_y, "Arial", 12.0f, false,
                          flex::Color{0.8f, 0.8f, 0.8f, 1.0f});
        
        const char* scales[] = {"1x", "2x", "3x"};
        for (int i = 0; i < 3; ++i) {
            float bx = value_x + i * 45;
            bool selected = (scale_ == i + 1);
            flex::Paint btn_bg = selected 
                ? flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.8f, 1.0f})
                : flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.25f, 1.0f});
            renderer.draw_rect(bx, row_y - 14, 38, 22, 4.0f, btn_bg, flex::Paint::none(), 0);
            renderer.draw_text(scales[i], bx + 12, row_y, "Arial", 11.0f, false,
                              flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
        }
        row_y += 35;
    }

    // Selection only checkbox
    renderer.draw_text("Selection only:", label_x, row_y, "Arial", 12.0f, false,
                       flex::Color{0.8f, 0.8f, 0.8f, 1.0f});
    
    flex::Paint check_bg = export_selection_only_
        ? flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.8f, 1.0f})
        : flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.25f, 1.0f});
    flex::Paint check_border = flex::Paint::solid(flex::Color{0.5f, 0.5f, 0.5f, 1.0f});
    renderer.draw_rect(value_x, row_y - 12, 18, 18, 3.0f, check_bg, check_border, 1.0f);
    if (export_selection_only_) {
        renderer.draw_text("✓", value_x + 3, row_y + 2, "Arial", 12.0f, true,
                          flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
    }

    row_y += 40;

    // Export button
    flex::Paint btn_bg = flex::Paint::solid(flex::Color{0.2f, 0.6f, 0.3f, 1.0f});
    renderer.draw_rect(x_ + 12, row_y - 10, width_ - 24, 32, 6.0f, btn_bg, flex::Paint::none(), 0);
    renderer.draw_text("Export", x_ + width_ / 2 - 20, row_y + 8, "Arial", 13.0f, true,
                       flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
}

bool ExportPanel::handle_click(float px, float py) {
    if (!visible_) return false;

    float row_y = y_ + 50;
    float value_x = x_ + 100;

    // Format buttons
    if (py >= row_y - 14 && py <= row_y + 8) {
        for (int i = 0; i < 2; ++i) {
            float bx = value_x + i * 60;
            if (px >= bx && px <= bx + 50) {
                selected_format_ = i;
                return true;
            }
        }
    }

    row_y += 35;

    // Scale buttons (PNG only)
    if (selected_format_ == 0 && py >= row_y - 14 && py <= row_y + 8) {
        for (int i = 0; i < 3; ++i) {
            float bx = value_x + i * 45;
            if (px >= bx && px <= bx + 38) {
                scale_ = i + 1;
                return true;
            }
        }
        row_y += 35;
    }

    // Selection checkbox
    float check_y = (selected_format_ == 0) ? y_ + 120 : y_ + 85;
    if (py >= check_y - 12 && py <= check_y + 6 && px >= value_x && px <= value_x + 18) {
        export_selection_only_ = !export_selection_only_;
        return true;
    }

    // Export button
    float btn_y = (selected_format_ == 0) ? y_ + 150 : y_ + 115;
    if (py >= btn_y && py <= btn_y + 32 && px >= x_ + 12 && px <= x_ + width_ - 12) {
        std::string format = (selected_format_ == 0) ? "png" : "svg";
        std::string path = export_path_ + "." + format;
        
        if (export_callback_) {
            export_callback_(path, format);
        } else if (format == "svg") {
            editor_->save_svg(path);
        }
        return true;
    }

    return false;
}

} // namespace meta_editor
