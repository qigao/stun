/*
 * Meta Editor - Style Panel
 *
 * UI panel for editing fill and stroke styles of selected objects.
 */

#pragma once

#include <flex.h>
#include <flexUI/widget.h>
#include <flexUI/types.h>
#include <functional>

namespace flexUI {
    class Box;
    class Element;
    class ColorPickerWidget;
    class GradientEditorWidget;
    class SliderWidget;
}

namespace meta_editor {

class SelectionManager;
class CommandManager;

enum class FillType { None, Solid, LinearGradient, RadialGradient };

class StylePanel : public flexUI::Widget {
public:
    StylePanel(SelectionManager* selection, CommandManager* commands);

    void render(const flexUI::Element& elem, flexUI::Renderer& renderer) override;
    bool handle_event(const flexUI::Event& event, flexUI::Element& elem) override;
    void update(float delta_ms, flexUI::Element& elem) override;
    const char* type_name() const override { return "StylePanel"; }

    void sync_from_selection();

    static void setup_ui(flexUI::Box* box, flexUI::Element* container,
                        SelectionManager* selection, CommandManager* commands);

private:
    void on_fill_type_changed(FillType type);
    void on_fill_color_changed(const flexUI::Color& color);
    void on_fill_gradient_changed();
    void on_stroke_toggle_changed(bool enabled);
    void on_stroke_color_changed(const flexUI::Color& color);
    void on_stroke_width_changed(float width);

    void apply_fill_to_selection();
    void apply_stroke_to_selection();
    void update_fill_ui_visibility();

    SelectionManager* selection_;
    CommandManager* commands_;

    FillType fill_type_ = FillType::Solid;
    flexUI::Color fill_color_{0.5f, 0.5f, 0.8f, 1.0f};
    flex::LinearGradient fill_linear_;
    flex::RadialGradient fill_radial_;

    bool stroke_enabled_ = true;
    flexUI::Color stroke_color_{0.2f, 0.2f, 0.2f, 1.0f};
    float stroke_width_ = 2.0f;

    bool syncing_ = false;

    // UI element references for visibility control
    flexUI::Element* fill_color_picker_elem_ = nullptr;
    flexUI::Element* fill_gradient_elem_ = nullptr;
    flexUI::Element* stroke_width_label_elem_ = nullptr;
    flexUI::GradientEditorWidget* gradient_widget_ = nullptr;
};

} // namespace meta_editor
