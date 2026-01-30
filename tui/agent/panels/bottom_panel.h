#pragma once
#include <flexUI/box.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>

using namespace flexUI;

class BottomPanel {
public:
    static Element* create(Box* box, Element* parent, InputWidget** out_input_widget, Element** out_input_wrapper) {
        auto* footer = box->create("div", "footer");
        footer->add_class("footer");
        parent->append(footer);
        
        auto* input_area = box->create("div", "input-area");
        input_area->add_class("input-area");
        footer->append(input_area);
        
        auto* input_wrapper = box->create_widget<InputWidget>("input", "main-input", "Type a message...");
        input_wrapper->add_class("input-box");
        
        *out_input_widget = dynamic_cast<InputWidget*>(input_wrapper->widget);
        *out_input_wrapper = input_wrapper;
        
        input_area->append(input_wrapper);
        input_area->append(box->create_widget<LabelWidget>("label", "", "Return to send | Ctrl+C to quit"))->add_class("input-hint");

        auto* status_bar = box->create("div", "status-bar");
        status_bar->add_class("status-bar");
        
        // Model Item
        auto* model_item = box->create("div", "status-model-item");
        model_item->add_class("status-item");
        model_item->append(box->create_widget<LabelWidget>("label", "", "MODEL:"))->add_class("status-label");
        auto* model_val = box->create_widget<LabelWidget>("label", "status-model", "GPT-4O");
        model_val->add_class("status-value");
        model_item->append(model_val);
        status_bar->append(model_item);

        // Status Item
        auto* status_item = box->create("div", "");
        status_item->add_class("status-item");
        status_item->append(box->create_widget<LabelWidget>("label", "", "STATUS:"))->add_class("status-label");
        auto* status_val = box->create_widget<LabelWidget>("label", "status-state", "READY");
        status_val->add_class("status-value");
        status_item->append(status_val);
        status_bar->append(status_item);

        // Messages Item
        auto* msg_item = box->create("div", "");
        msg_item->add_class("status-item");
        msg_item->append(box->create_widget<LabelWidget>("label", "", "MESSAGES:"))->add_class("status-label");
        auto* msg_val = box->create_widget<LabelWidget>("label", "status-count", "0");
        msg_val->add_class("status-value");
        msg_item->append(msg_val);
        status_bar->append(msg_item);

        footer->append(status_bar);
        
        return footer;
    }
};
