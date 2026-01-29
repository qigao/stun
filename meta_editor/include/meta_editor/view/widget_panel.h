/*
 * Meta Editor - Widget Panel
 *
 * A panel using flexUI accordion to organize widgets.
 * Provides collapsible sections for different widget categories.
 */

#pragma once

#include "panel.h"
#include <flexUI/box.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <functional>
#include <string>
#include <memory>

namespace meta_editor {

class Canvas;
class SelectionManager;

class WidgetPanel : public Panel {
public:
    WidgetPanel(flex::Renderer* renderer, Canvas* canvas, SelectionManager* selection);
    ~WidgetPanel();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    // Update panel content based on selection
    void update_from_selection();

    // Callbacks
    using ValueChangeCallback = std::function<void(const std::string& property, float value)>;
    using ColorChangeCallback = std::function<void(const std::string& property, const flex::Color& color)>;
    
    void set_value_callback(ValueChangeCallback cb) { on_value_change_ = std::move(cb); }
    void set_color_callback(ColorChangeCallback cb) { on_color_change_ = std::move(cb); }

private:
    void build_ui();
    void build_transform_section();
    void build_fill_section();
    void build_stroke_section();
    void build_effects_section();

    flexUI::Box* box_ = nullptr;
    flexUI::Element* root_ = nullptr;
    flexUI::Element* accordion_elem_ = nullptr;

    // Transform controls
    flexUI::Element* pos_x_input_ = nullptr;
    flexUI::Element* pos_y_input_ = nullptr;
    flexUI::Element* width_input_ = nullptr;
    flexUI::Element* height_input_ = nullptr;
    flexUI::Element* rotation_slider_ = nullptr;

    // Fill controls
    flexUI::Element* fill_color_ = nullptr;
    flexUI::Element* fill_opacity_ = nullptr;

    // Stroke controls
    flexUI::Element* stroke_color_ = nullptr;
    flexUI::Element* stroke_width_ = nullptr;

    // Effects
    flexUI::Element* opacity_slider_ = nullptr;
    flexUI::Element* blur_slider_ = nullptr;

    Canvas* canvas_;
    SelectionManager* selection_;

    ValueChangeCallback on_value_change_;
    ColorChangeCallback on_color_change_;
};

} // namespace meta_editor
