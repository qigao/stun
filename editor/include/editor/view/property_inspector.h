/*
 * PropertyInspector - Inspects and edits EditorNode properties
 */

#pragma once

#include <editor/model/node.h>
#include <editor/core/observable.h>
#include <tvgbox2/box.h>
#include <tvgbox2/element.h>
#include <tvgbox2/widgets/label_widget.h>
#include <tvgbox2/widgets/input_widget.h>
#include <tvgbox2/widgets/input_widget.h>
#include <tvgbox2/widgets/switch_widget.h>
#include <tvgbox2/widgets/dropdown_widget.h>
#include <tvgbox2/widgets/colorpicker_widget.h>
#include <tvgbox2/widgets/slider_widget.h>
#include <tvgbox2/computed_style.h>

namespace editor {

class PropertyInspector {
public:
    PropertyInspector(tvgbox2::Box* box, tvgbox2::Element* parent) 
        : box_(box), parent_elem_(parent) {
        
        // Create container
        root_ = box_->create("div", "property-inspector");
        
        // Style
        root_->computed_style->position = tvgbox2::Position::Absolute;
        root_->computed_style->right = 0;
        root_->computed_style->top = 0;
        root_->computed_style->width = 250;
        root_->computed_style->height_is_percent = true; 
        root_->computed_style->height = 100; // 100%
        root_->computed_style->background_color = {42, 42, 42, 255}; // Dark theme
        root_->computed_style->padding[0] = 10; root_->computed_style->padding[1] = 10;
        root_->computed_style->padding[2] = 10; root_->computed_style->padding[3] = 10;
        root_->computed_style->flex_direction = tvgbox2::FlexDirection::Column;
        root_->computed_style->gap = 10;

        parent_elem_->append(root_);
    }

    ~PropertyInspector() {
        if (target_ && observer_id_ > 0) {
             target_->removeObserver(observer_id_);
        }
    }

    void setTarget(EditorNode::Ptr target) {
        if (target_ == target) return;

        if (target_ && observer_id_ > 0) {
             target_->removeObserver(observer_id_);
             observer_id_ = 0;
        }
        
        target_ = target;
        
        if (target_) {
            observer_id_ = target_->addObserver([this](EventType type, void* data) {
                onNotify(type, data);
            });
        }

        updateFromTarget();
    }

    void onNotify(EventType type, void* sender) {
        if (sender == target_.get() && type == EventType::NodeModified) {
            // Update values only, avoiding full rebuild if possible
            // For now, full rebuild is safer/easier
            updateFromTarget();
        }
    }

private:
    void updateFromTarget() {
        // Clear existing children
        root_->children.clear();
        root_->mark_layout_dirty();

        if (!target_) {
            auto* label = box_->create_widget<tvgbox2::LabelWidget>("label", "", "No Selection");
            label->computed_style->text_color = {136, 136, 136, 255}; // #888
            root_->append(label);
            return;
        }

        auto* title = box_->create_widget<tvgbox2::LabelWidget>("label", "", "Properties");
        title->computed_style->font_weight = tvgbox2::FontWeight::Bold;
        title->computed_style->margin[2] = 10; // bottom margin
        title->computed_style->text_color = {221, 221, 221, 255}; // #DDD
        root_->append(title);

        auto props = target_->getProperties();
        for (const auto& pd : props) {
            buildPropertyControl(pd);
        }
    }

    void buildPropertyControl(const PropertyDescriptor& pd) {
        auto* row = box_->create("div", "");
        row->computed_style->flex_direction = tvgbox2::FlexDirection::Row;
        row->computed_style->justify_content = tvgbox2::JustifyContent::SpaceBetween;
        row->computed_style->align_items = tvgbox2::AlignItems::Center;
        row->computed_style->margin[2] = 5; // bottom

        auto* label = box_->create_widget<tvgbox2::LabelWidget>("label", "", pd.label);
        label->computed_style->text_color = {204, 204, 204, 255};
        label->computed_style->font_size = 12;
        label->computed_style->width = 80;
        row->append(label);

        std::string currentVal = target_->getPropertyValue(pd.name);

        if (pd.type == "float") {
            if (pd.min > -1e20f && pd.max < 1e20f) {
                // Use Slider for bounded floats
                float val = 0.0f;
                try { val = std::stof(currentVal); } catch (...) {}
                
                auto* slid = box_->create_widget<tvgbox2::SliderWidget>("slider", "", val, pd.min, pd.max, pd.step);
                slid->computed_style->width = 100;
                auto* widget = static_cast<tvgbox2::SliderWidget*>(slid->widget);
                
                widget->set_change_callback([this, name=pd.name](float v) {
                    if (target_) target_->setPropertyValue(name, std::to_string(v));
                });
                row->append(slid);
            } else {
                // Use Input for unbounded floats
                auto* input = box_->create_widget<tvgbox2::InputWidget>("input", "", currentVal);
                input->computed_style->width = 100;
                static_cast<tvgbox2::InputWidget*>(input->widget)->set_change_callback([this, name=pd.name](const std::string& val) {
                    if (target_) target_->setPropertyValue(name, val);
                });
                row->append(input);
            }
        } 
        else if (pd.type == "int") {
             auto* input = box_->create_widget<tvgbox2::InputWidget>("input", "", currentVal);
             input->computed_style->width = 100;
             static_cast<tvgbox2::InputWidget*>(input->widget)->set_change_callback([this, name=pd.name](const std::string& val) {
                if (target_) target_->setPropertyValue(name, val);
             });
             row->append(input);
        } 
        else if (pd.type == "string") {
             auto* input = box_->create_widget<tvgbox2::InputWidget>("input", "", currentVal);
             input->computed_style->width = 100;
             static_cast<tvgbox2::InputWidget*>(input->widget)->set_change_callback([this, name=pd.name](const std::string& val) {
                if (target_) target_->setPropertyValue(name, val);
             });
             row->append(input);
        }
        else if (pd.type == "bool") {
            bool val = (currentVal == "true");
            auto* sw = box_->create_widget<tvgbox2::SwitchWidget>("switch", "", "", val);
            static_cast<tvgbox2::SwitchWidget*>(sw->widget)->set_change_callback([this, name=pd.name](bool checked) {
                if (target_) target_->setPropertyValue(name, checked ? "true" : "false");
            });
            row->append(sw);
        }
        else if (pd.type == "enum") {
            auto* dd = box_->create_widget<tvgbox2::DropdownWidget>("dropdown", "", "Select...");
            dd->computed_style->width = 100;
            auto* widget = static_cast<tvgbox2::DropdownWidget*>(dd->widget);
            
            for (const auto& opt : pd.options) {
                widget->add_option(opt, opt); // Use label as value
            }
            widget->set_selected_value(currentVal);
            
            widget->set_change_callback([this, name=pd.name](const std::string& val) {
                if (target_) target_->setPropertyValue(name, val);
            });
            row->append(dd);
        }
        else if (pd.type == "color") {
             // For color, currentVal is hex or similar?
             auto* picker = box_->create_widget<tvgbox2::ColorPickerWidget>("colorpicker", "");
             picker->computed_style->width = 100;
             picker->computed_style->height = 24;
             
             auto* widget = static_cast<tvgbox2::ColorPickerWidget*>(picker->widget);
             
             // Initial value from hex
             if (!currentVal.empty() && currentVal[0] == '#') {
                  // Re-use logic? Or assume tvgbox2::Color matches
                  // We need hex parser here too unless we add a helper to ColorPickerWidget
                  // But we don't have hexToColor exposed here.
                  // Simple parser for PropertyInspector
                  unsigned int r=0,g=0,b=0,a=255;
                  std::string raw = currentVal.substr(1);
                  if (raw.length() == 8) sscanf(raw.c_str(), "%02x%02x%02x%02x", &r, &g, &b, &a);
                  else if (raw.length() == 6) sscanf(raw.c_str(), "%02x%02x%02x", &r, &g, &b);
                  widget->set_color({(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
             }
             
             widget->set_change_callback([this, name=pd.name](const tvgbox2::Color& c) {
                 // Convert back to hex
                 char buf[10];
                 if (c.a == 255) sprintf(buf, "#%02x%02x%02x", c.r, c.g, c.b);
                 else sprintf(buf, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
                 
                 if (target_) target_->setPropertyValue(name, buf);
             });

             row->append(picker);
        }

        root_->append(row);
    }

    tvgbox2::Box* box_;
    tvgbox2::Element* parent_elem_;
    tvgbox2::Element* root_;
    EditorNode::Ptr target_;
    Observable::ObserverId observer_id_ = 0;
};

} // namespace editor
