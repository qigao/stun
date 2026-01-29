/*
 * Meta Editor - Template Panel
 *
 * Preset templates for quick whiteboard setup.
 */

#pragma once

#include "panel.h"
#include <functional>
#include <string>
#include <vector>

namespace meta_editor {

class Editor;

struct Template {
    std::string name;
    std::string icon;
    std::string description;
};

class TemplatePanel : public Panel {
public:
    explicit TemplatePanel(Editor* editor);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    using ApplyCallback = std::function<void(const std::string& template_name)>;
    void set_apply_callback(ApplyCallback cb) { apply_callback_ = std::move(cb); }

protected:
    float content_height() const override { return 350; }

private:
    void apply_template(int index);

    Editor* editor_;
    std::vector<Template> templates_;
    int hovered_index_ = -1;
    
    ApplyCallback apply_callback_;
};

} // namespace meta_editor
