/*
 * Meta Editor - Widget Panel Implementation
 */

#include "meta_editor/view/widget_panel.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"

namespace meta_editor {

static const char* WIDGET_PANEL_CSS = R"(
#widget-root {
    display: flex;
    flex-direction: column;
    width: 100%;
    height: 100%;
    background: transparent;
}

.section-content {
    display: flex;
    flex-direction: column;
    padding: 8px;
    gap: 6px;
}

.prop-row {
    display: flex;
    align-items: center;
    gap: 8px;
}

.prop-label {
    color: #a0a0a0;
    font-size: 11px;
    width: 50px;
}

.prop-input {
    flex: 1;
    background: #252530;
    border: 1px solid #3a3a4a;
    color: #ffffff;
    padding: 4px 6px;
    border-radius: 3px;
    font-size: 11px;
}

.prop-slider {
    flex: 1;
}

.color-preview {
    width: 24px;
    height: 24px;
    border-radius: 4px;
    border: 1px solid #555;
}
)";

WidgetPanel::WidgetPanel(flex::Renderer* renderer, Canvas* canvas, SelectionManager* selection)
    : canvas_(canvas), selection_(selection) {
    width_ = 240;
    height_ = 400;

    box_ = new flexUI::Box(renderer);
    box_->set_viewport(width_, height_);
    build_ui();
}

WidgetPanel::~WidgetPanel() {
    delete box_;
}

void WidgetPanel::build_ui() {
    box_->load_css(WIDGET_PANEL_CSS);

    root_ = box_->create("div", "widget-root");
    box_->set_root(root_);

    // Create accordion
    accordion_elem_ = box_->create_widget<flexUI::AccordionWidget>("accordion", "main-accordion");
    auto* accordion = static_cast<flexUI::AccordionWidget*>(accordion_elem_->widget);
    
    accordion->add_section("Transform", "transform", 120);
    accordion->add_section("Fill", "fill", 80);
    accordion->add_section("Stroke", "stroke", 80);
    accordion->add_section("Effects", "effects", 80);
    
    accordion->expand("transform");
    accordion->set_allow_multiple(true);

    root_->append(accordion_elem_);

    build_transform_section();
    build_fill_section();
    build_stroke_section();
    build_effects_section();
}

void WidgetPanel::build_transform_section() {
    auto* content = box_->create("div", "transform-content");
    content->add_class("section-content");

    // X position
    auto* row_x = box_->create("div", "");
    row_x->add_class("prop-row");
    auto* lbl_x = box_->create_widget<flexUI::LabelWidget>("label", "", "X");
    lbl_x->add_class("prop-label");
    row_x->append(lbl_x);
    pos_x_input_ = box_->create_widget<flexUI::InputWidget>("input", "pos-x", "0");
    pos_x_input_->add_class("prop-input");
    row_x->append(pos_x_input_);
    content->append(row_x);

    // Y position
    auto* row_y = box_->create("div", "");
    row_y->add_class("prop-row");
    auto* lbl_y = box_->create_widget<flexUI::LabelWidget>("label", "", "Y");
    lbl_y->add_class("prop-label");
    row_y->append(lbl_y);
    pos_y_input_ = box_->create_widget<flexUI::InputWidget>("input", "pos-y", "0");
    pos_y_input_->add_class("prop-input");
    row_y->append(pos_y_input_);
    content->append(row_y);

    // Width
    auto* row_w = box_->create("div", "");
    row_w->add_class("prop-row");
    auto* lbl_w = box_->create_widget<flexUI::LabelWidget>("label", "", "W");
    lbl_w->add_class("prop-label");
    row_w->append(lbl_w);
    width_input_ = box_->create_widget<flexUI::InputWidget>("input", "width", "100");
    width_input_->add_class("prop-input");
    row_w->append(width_input_);
    content->append(row_w);

    // Height
    auto* row_h = box_->create("div", "");
    row_h->add_class("prop-row");
    auto* lbl_h = box_->create_widget<flexUI::LabelWidget>("label", "", "H");
    lbl_h->add_class("prop-label");
    row_h->append(lbl_h);
    height_input_ = box_->create_widget<flexUI::InputWidget>("input", "height", "100");
    height_input_->add_class("prop-input");
    row_h->append(height_input_);
    content->append(row_h);

    // Rotation
    auto* row_r = box_->create("div", "");
    row_r->add_class("prop-row");
    auto* lbl_r = box_->create_widget<flexUI::LabelWidget>("label", "", "Rot");
    lbl_r->add_class("prop-label");
    row_r->append(lbl_r);
    rotation_slider_ = box_->create_widget<flexUI::SliderWidget>("slider", "rotation", 0.0f, 360.0f, 0.0f);
    rotation_slider_->add_class("prop-slider");
    row_r->append(rotation_slider_);
    content->append(row_r);

    root_->append(content);
}

void WidgetPanel::build_fill_section() {
    auto* content = box_->create("div", "fill-content");
    content->add_class("section-content");

    // Fill color
    auto* row_c = box_->create("div", "");
    row_c->add_class("prop-row");
    auto* lbl_c = box_->create_widget<flexUI::LabelWidget>("label", "", "Color");
    lbl_c->add_class("prop-label");
    row_c->append(lbl_c);
    fill_color_ = box_->create_widget<flexUI::ColorPickerWidget>("colorpicker", "fill-color");
    row_c->append(fill_color_);
    content->append(row_c);

    // Fill opacity
    auto* row_o = box_->create("div", "");
    row_o->add_class("prop-row");
    auto* lbl_o = box_->create_widget<flexUI::LabelWidget>("label", "", "Opacity");
    lbl_o->add_class("prop-label");
    row_o->append(lbl_o);
    fill_opacity_ = box_->create_widget<flexUI::SliderWidget>("slider", "fill-opacity", 0.0f, 100.0f, 100.0f);
    fill_opacity_->add_class("prop-slider");
    row_o->append(fill_opacity_);
    content->append(row_o);

    root_->append(content);
}

void WidgetPanel::build_stroke_section() {
    auto* content = box_->create("div", "stroke-content");
    content->add_class("section-content");

    // Stroke color
    auto* row_c = box_->create("div", "");
    row_c->add_class("prop-row");
    auto* lbl_c = box_->create_widget<flexUI::LabelWidget>("label", "", "Color");
    lbl_c->add_class("prop-label");
    row_c->append(lbl_c);
    stroke_color_ = box_->create_widget<flexUI::ColorPickerWidget>("colorpicker", "stroke-color");
    row_c->append(stroke_color_);
    content->append(row_c);

    // Stroke width
    auto* row_w = box_->create("div", "");
    row_w->add_class("prop-row");
    auto* lbl_w = box_->create_widget<flexUI::LabelWidget>("label", "", "Width");
    lbl_w->add_class("prop-label");
    row_w->append(lbl_w);
    stroke_width_ = box_->create_widget<flexUI::SliderWidget>("slider", "stroke-width", 0.0f, 20.0f, 1.0f);
    stroke_width_->add_class("prop-slider");
    row_w->append(stroke_width_);
    content->append(row_w);

    root_->append(content);
}

void WidgetPanel::build_effects_section() {
    auto* content = box_->create("div", "effects-content");
    content->add_class("section-content");

    // Opacity
    auto* row_o = box_->create("div", "");
    row_o->add_class("prop-row");
    auto* lbl_o = box_->create_widget<flexUI::LabelWidget>("label", "", "Opacity");
    lbl_o->add_class("prop-label");
    row_o->append(lbl_o);
    opacity_slider_ = box_->create_widget<flexUI::SliderWidget>("slider", "opacity", 0.0f, 100.0f, 100.0f);
    opacity_slider_->add_class("prop-slider");
    row_o->append(opacity_slider_);
    content->append(row_o);

    // Blur
    auto* row_b = box_->create("div", "");
    row_b->add_class("prop-row");
    auto* lbl_b = box_->create_widget<flexUI::LabelWidget>("label", "", "Blur");
    lbl_b->add_class("prop-label");
    row_b->append(lbl_b);
    blur_slider_ = box_->create_widget<flexUI::SliderWidget>("slider", "blur", 0.0f, 50.0f, 0.0f);
    blur_slider_->add_class("prop-slider");
    row_b->append(blur_slider_);
    content->append(row_b);

    root_->append(content);
}

void WidgetPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);

    renderer.save();
    renderer.translate(x_, y_);
    box_->update();
    renderer.restore();
}

bool WidgetPanel::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;

    float local_x = screen_x - x_;
    float local_y = screen_y - y_;

    auto event = flexUI::Event::mouse_down(local_x, local_y, flexUI::MouseButton::Left);
    box_->dispatch_event(event);

    auto up_event = flexUI::Event::mouse_up(local_x, local_y, flexUI::MouseButton::Left);
    box_->dispatch_event(up_event);

    return true;
}

void WidgetPanel::update_from_selection() {
    if (!selection_ || !selection_->has_selection()) return;

    auto* node = selection_->primary_selection();
    if (!node) return;

    // Update position inputs
    if (pos_x_input_) {
        static_cast<flexUI::InputWidget*>(pos_x_input_->widget)->set_text(
            std::to_string((int)node->x()));
    }
    if (pos_y_input_) {
        static_cast<flexUI::InputWidget*>(pos_y_input_->widget)->set_text(
            std::to_string((int)node->y()));
    }

    // Update size inputs
    auto bounds = node->bounds();
    if (width_input_) {
        static_cast<flexUI::InputWidget*>(width_input_->widget)->set_text(
            std::to_string((int)bounds.width));
    }
    if (height_input_) {
        static_cast<flexUI::InputWidget*>(height_input_->widget)->set_text(
            std::to_string((int)bounds.height));
    }

    // Update rotation
    if (rotation_slider_) {
        static_cast<flexUI::SliderWidget*>(rotation_slider_->widget)->set_value(node->rotation());
    }

    // Update opacity
    if (opacity_slider_) {
        static_cast<flexUI::SliderWidget*>(opacity_slider_->widget)->set_value(node->opacity() * 100.0f);
    }
}

} // namespace meta_editor
