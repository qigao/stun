/*
 * Meta Editor - Style Panel Implementation
 */

#include "meta_editor/view/style_panel.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/renderer.h>
#include <flexUI/computed_style.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <flexUI/widgets/gradient_editor_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/button_widget.h>
#include <stb_sprintf.h>
#include <algorithm>

namespace meta_editor {

StylePanel::StylePanel(SelectionManager* selection, CommandManager* commands)
    : selection_(selection), commands_(commands) {
    fill_linear_.stops.push_back({0.0f, flex::Color::Black});
    fill_linear_.stops.push_back({1.0f, flex::Color::White});
    fill_radial_.stops.push_back({0.0f, flex::Color::White});
    fill_radial_.stops.push_back({1.0f, flex::Color::Black});
}

void StylePanel::render(const flexUI::Element& elem, flexUI::Renderer& renderer) {
    auto& r = renderer.flex();

    r.draw_rect(0, 0, elem.width(), elem.height(), 0,
                flexUI::Paint::solid(flexUI::Color{0.13f, 0.13f, 0.13f, 1.0f}),
                flexUI::Paint::none(), 0);
}

bool StylePanel::handle_event(const flexUI::Event& event, flexUI::Element& elem) {
    return false;
}

void StylePanel::update(float delta_ms, flexUI::Element& elem) {
}

void StylePanel::sync_from_selection() {
    if (syncing_) return;
    syncing_ = true;

    auto fill = selection_->get_common_fill();
    if (fill.has_value()) {
        switch (fill->type) {
            case flex::Paint::Type::None:
                fill_type_ = FillType::None;
                break;
            case flex::Paint::Type::Solid:
                fill_type_ = FillType::Solid;
                fill_color_ = {fill->color.r, fill->color.g, fill->color.b, fill->color.a};
                break;
            case flex::Paint::Type::Linear:
                fill_type_ = FillType::LinearGradient;
                fill_linear_ = fill->linear;
                break;
            case flex::Paint::Type::Radial:
                fill_type_ = FillType::RadialGradient;
                fill_radial_ = fill->radial;
                break;
        }
    }

    auto stroke = selection_->get_common_stroke();
    if (stroke.has_value()) {
        stroke_enabled_ = (stroke->type != flex::Paint::Type::None);
        if (stroke->type == flex::Paint::Type::Solid) {
            stroke_color_ = {stroke->color.r, stroke->color.g, stroke->color.b, stroke->color.a};
        }
    }

    auto width = selection_->get_common_stroke_width();
    if (width.has_value()) {
        stroke_width_ = *width;
    }

    syncing_ = false;
}

void StylePanel::on_fill_type_changed(FillType type) {
    fill_type_ = type;
    update_fill_ui_visibility();
    apply_fill_to_selection();
}

void StylePanel::on_fill_color_changed(const flexUI::Color& color) {
    fill_color_ = color;
    if (fill_type_ == FillType::Solid) {
        apply_fill_to_selection();
    }
}

void StylePanel::on_fill_gradient_changed() {
    if (gradient_widget_) {
        if (fill_type_ == FillType::LinearGradient) {
            fill_linear_ = gradient_widget_->linear_gradient();
        } else if (fill_type_ == FillType::RadialGradient) {
            fill_radial_ = gradient_widget_->radial_gradient();
        }
    }
    apply_fill_to_selection();
}

void StylePanel::on_stroke_toggle_changed(bool enabled) {
    stroke_enabled_ = enabled;
    apply_stroke_to_selection();
}

void StylePanel::on_stroke_color_changed(const flexUI::Color& color) {
    stroke_color_ = color;
    if (stroke_enabled_) {
        apply_stroke_to_selection();
    }
}

void StylePanel::on_stroke_width_changed(float width) {
    stroke_width_ = width;
    if (stroke_width_label_elem_) {
        char buf[16];
        stbsp_snprintf(buf, sizeof(buf), "%.1f", width);
        stroke_width_label_elem_->set_text(buf);
    }
    if (stroke_enabled_) {
        apply_stroke_to_selection();
    }
}

void StylePanel::update_fill_ui_visibility() {
    if (!fill_color_picker_elem_ || !fill_gradient_elem_) return;

    bool show_color = (fill_type_ == FillType::Solid);
    bool show_gradient = (fill_type_ == FillType::LinearGradient || fill_type_ == FillType::RadialGradient);

    fill_color_picker_elem_->computed_style->display = show_color ? flexUI::Display::Flex : flexUI::Display::None;
    fill_gradient_elem_->computed_style->display = show_gradient ? flexUI::Display::Flex : flexUI::Display::None;

    if (show_gradient && gradient_widget_) {
        if (fill_type_ == FillType::LinearGradient) {
            gradient_widget_->set_type(flexUI::GradientEditorWidget::GradientType::Linear);
            gradient_widget_->set_gradient(fill_linear_);
        } else {
            gradient_widget_->set_type(flexUI::GradientEditorWidget::GradientType::Radial);
            gradient_widget_->set_gradient(fill_radial_);
        }
    }
}

void StylePanel::apply_fill_to_selection() {
    if (!selection_->has_selection()) return;

    std::vector<flex::Node*> nodes = selection_->selection();
    std::vector<flex::Paint> old_fills;

    for (auto* node : nodes) {
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            if (shape->has_fill()) {
                auto f = shape->fill();
                if (f.type == flex::FillType::Solid) {
                    old_fills.push_back(flex::Paint::solid(f.color));
                } else if (f.type == flex::FillType::LinearGradient) {
                    old_fills.push_back(flex::Paint(f.linear_gradient));
                } else if (f.type == flex::FillType::RadialGradient) {
                    old_fills.push_back(flex::Paint(f.radial_gradient));
                } else {
                    old_fills.push_back(flex::Paint::none());
                }
            } else {
                old_fills.push_back(flex::Paint::none());
            }
        } else {
            old_fills.push_back(flex::Paint::none());
        }
    }

    flex::Paint new_fill;
    switch (fill_type_) {
        case FillType::None:
            new_fill = flex::Paint::none();
            break;
        case FillType::Solid:
            new_fill = flex::Paint::solid(flex::Color(fill_color_.r, fill_color_.g, fill_color_.b, fill_color_.a));
            break;
        case FillType::LinearGradient:
            new_fill = flex::Paint(fill_linear_);
            break;
        case FillType::RadialGradient:
            new_fill = flex::Paint(fill_radial_);
            break;
    }

    auto cmd = std::make_unique<SetFillCommand>(nodes, old_fills, new_fill);
    commands_->execute(std::move(cmd));
}

void StylePanel::apply_stroke_to_selection() {
    if (!selection_->has_selection()) return;

    std::vector<flex::Node*> nodes = selection_->selection();
    std::vector<flex::Paint> old_strokes;
    std::vector<float> old_widths;

    for (auto* node : nodes) {
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            if (shape->has_stroke()) {
                auto s = shape->stroke();
                old_widths.push_back(s.width);
                if (s.type == flex::StrokeType::Solid) {
                    old_strokes.push_back(flex::Paint::solid(s.color));
                } else {
                    old_strokes.push_back(flex::Paint::none());
                }
            } else {
                old_strokes.push_back(flex::Paint::none());
                old_widths.push_back(1.0f);
            }
        } else {
            old_strokes.push_back(flex::Paint::none());
            old_widths.push_back(1.0f);
        }
    }

    flex::Paint new_stroke;
    if (stroke_enabled_) {
        new_stroke = flex::Paint::solid(flex::Color(stroke_color_.r, stroke_color_.g, stroke_color_.b, stroke_color_.a));
    } else {
        new_stroke = flex::Paint::none();
    }

    auto cmd = std::make_unique<SetStrokeCommand>(nodes, old_strokes, old_widths, new_stroke, stroke_width_);
    commands_->execute(std::move(cmd));
}

void StylePanel::setup_ui(flexUI::Box* box, flexUI::Element* container,
                          SelectionManager* selection, CommandManager* commands) {

    auto* panel = new StylePanel(selection, commands);

    // CSS is loaded from assets/editor.css

    auto* panel_elem = box->create_with_widget("div", panel, "style_panel_root");
    panel_elem->add_class("style_panel");
    container->append(panel_elem);

    // Fill section
    auto* fill_section = box->create("div", "fill_section");
    fill_section->add_class("style_section");
    panel_elem->append(fill_section);

    auto* fill_label = box->create("div", "");
    fill_label->set_text("FILL");
    fill_label->add_class("style_section_label");
    fill_section->append(fill_label);

    auto* fill_type_btns = box->create("div", "fill_type_btns");
    fill_type_btns->add_class("fill_type_buttons");
    fill_section->append(fill_type_btns);

    auto create_fill_btn = [&](const char* text, const char* id, FillType type) {
        auto* btn = box->create("button", id);
        btn->set_text(text);
        btn->add_class("fill_type_btn");
        if (type == FillType::Solid) btn->add_class("active");
        btn->on_click([panel, box, type]() {
            for (auto* child : box->get_by_id("fill_type_btns")->child_elements()) {
                child->remove_class("active");
            }
            box->get_by_id(type == FillType::None ? "fill_none" :
                          type == FillType::Solid ? "fill_solid" :
                          type == FillType::LinearGradient ? "fill_linear" : "fill_radial")->add_class("active");
            panel->on_fill_type_changed(type);
        });
        fill_type_btns->append(btn);
    };

    create_fill_btn("None", "fill_none", FillType::None);
    create_fill_btn("Solid", "fill_solid", FillType::Solid);
    create_fill_btn("Linear", "fill_linear", FillType::LinearGradient);
    create_fill_btn("Radial", "fill_radial", FillType::RadialGradient);

    // Color picker
    auto* color_picker_widget = new flexUI::ColorPickerWidget();
    color_picker_widget->set_color(panel->fill_color_);
    color_picker_widget->set_change_callback([panel](const flexUI::Color& c) {
        panel->on_fill_color_changed(c);
    });
    auto* color_picker = box->create_with_widget("div", color_picker_widget, "fill_color_picker");
    color_picker->add_class("style_color_picker");
    fill_section->append(color_picker);
    panel->fill_color_picker_elem_ = color_picker;

    // Gradient editor
    auto* gradient_widget = new flexUI::GradientEditorWidget();
    gradient_widget->set_change_callback([panel]() {
        panel->on_fill_gradient_changed();
    });
    auto* gradient_editor = box->create_with_widget("div", gradient_widget, "fill_gradient");
    gradient_editor->add_class("style_gradient_editor");
    gradient_editor->computed_style->display = flexUI::Display::None;
    fill_section->append(gradient_editor);
    panel->fill_gradient_elem_ = gradient_editor;
    panel->gradient_widget_ = gradient_widget;

    // Stroke section
    auto* stroke_section = box->create("div", "stroke_section");
    stroke_section->add_class("style_section");
    panel_elem->append(stroke_section);

    auto* stroke_label = box->create("div", "");
    stroke_label->set_text("STROKE");
    stroke_label->add_class("style_section_label");
    stroke_section->append(stroke_label);

    auto* stroke_row = box->create("div", "stroke_row");
    stroke_row->add_class("stroke_controls");
    stroke_section->append(stroke_row);

    auto* stroke_toggle = box->create("button", "stroke_toggle");
    stroke_toggle->add_class("stroke_toggle_btn");
    stroke_toggle->add_class("active");
    stroke_toggle->on_click([panel, stroke_toggle]() {
        bool enabled = !stroke_toggle->has_class("active");
        if (enabled) {
            stroke_toggle->add_class("active");
        } else {
            stroke_toggle->remove_class("active");
        }
        panel->on_stroke_toggle_changed(enabled);
    });
    stroke_row->append(stroke_toggle);

    // Stroke width slider
    auto* width_slider_widget = new flexUI::SliderWidget(0.5f, 20.0f, panel->stroke_width_, 0.5f);
    width_slider_widget->set_change_callback([panel](float v) {
        panel->on_stroke_width_changed(v);
    });
    auto* width_slider = box->create_with_widget("div", width_slider_widget, "stroke_width_slider");
    width_slider->add_class("stroke_width_slider");
    stroke_row->append(width_slider);

    auto* width_label = box->create("div", "stroke_width_label");
    width_label->set_text("2.0");
    width_label->add_class("stroke_width_label");
    stroke_row->append(width_label);
    panel->stroke_width_label_elem_ = width_label;

    // Stroke color picker
    auto* stroke_color_widget = new flexUI::ColorPickerWidget();
    stroke_color_widget->set_color(panel->stroke_color_);
    stroke_color_widget->set_change_callback([panel](const flexUI::Color& c) {
        panel->on_stroke_color_changed(c);
    });
    auto* stroke_color_picker = box->create_with_widget("div", stroke_color_widget, "stroke_color_picker");
    stroke_color_picker->add_class("style_color_picker");
    stroke_section->append(stroke_color_picker);

    // Selection change callback
    selection->set_selection_change_callback([panel]() {
        panel->sync_from_selection();
    });
}

} // namespace meta_editor
