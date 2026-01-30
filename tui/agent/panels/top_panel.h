#pragma once
#include <flexUI/box.h>
#include <flexUI/widgets/label_widget.h>

using namespace flexUI;

class TopPanel {
public:
    static Element* create(Box* box, Element* parent) {
        auto* header = box->create("div", "header");
        header->add_class("header");
        parent->append(header);
        
        header->append(box->create_widget<LabelWidget>("label", "", "AGENT CLI"))->add_class("header-title");
        header->append(box->create_widget<LabelWidget>("label", "", "MVC v1.0"))->add_class("header-right");
        
        return header;
    }
};
