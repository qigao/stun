/*
 * Meta Editor - Export Panel
 *
 * UI panel for exporting canvas to PNG/SVG.
 */

#pragma once

#include "panel.h"
#include <functional>
#include <string>

namespace meta_editor {

class Editor;

class ExportPanel : public Panel {
public:
    explicit ExportPanel(Editor* editor);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    const char* title() const { return "Export"; }

    using ExportCallback = std::function<void(const std::string& path, const std::string& format)>;
    void set_export_callback(ExportCallback cb) { export_callback_ = std::move(cb); }

    void set_export_path(const std::string& path) { export_path_ = path; }

protected:
    float content_height() const override { return 200; }

private:
    Editor* editor_;
    std::string export_path_ = "export";
    int selected_format_ = 0;  // 0=PNG, 1=SVG
    int scale_ = 1;  // 1x, 2x, 3x
    bool export_selection_only_ = false;
    
    ExportCallback export_callback_;
};

} // namespace meta_editor
