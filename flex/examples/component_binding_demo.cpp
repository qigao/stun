/*
 * Component Binding Demo
 * Demonstrates ${$input} bindings for component props and layout sync.
 */

#include <flex.h>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace flex;

static void register_demo_component() {
    auto comp = Component::create("BadgeBinding");
    comp->add_prop("width", 10.0f);
    comp->add_prop("height", 10.0f);
    comp->set_builder([](const Props& props) -> ComponentNodePtr {
        auto shape = Shape::create();
        float w = get_prop_float(props, "width", 10.0f);
        float h = get_prop_float(props, "height", 10.0f);
        shape->set_rect(w, h);
        shape->set_layout_width(w);
        shape->set_layout_height(h);
        shape->set_fill(Color::from_hex("#00d9ff"));
        return shape;
    });
    ComponentRegistry::instance().register_component(comp);
}

static void print_state(Scene::RawPtr scene, const char* label) {
    auto* badge = dynamic_cast<Shape*>(scene->find("badge"));
    auto* box = dynamic_cast<Shape*>(scene->find("box"));

    std::cout << label << "\n";
    if (badge) {
        std::cout << "  badge rect: " << badge->rect().width << " x " << badge->rect().height
                  << " | layout: " << badge->layout_width() << " x " << badge->layout_height() << "\n";
    }
    if (box) {
        std::cout << "  box rect: " << box->rect().width << " x " << box->rect().height
                  << " | layout: " << box->layout_width() << " x " << box->layout_height() << "\n";
    }
}

int main() {
    std::cout << "Component Binding Demo - Flex Engine\n";

    register_demo_component();

    std::ifstream flex_file("component_binding.flex");
    if (!flex_file.is_open()) {
        std::cerr << "Failed to open component_binding.flex\n";
        return 1;
    }

    std::stringstream flex_buffer;
    flex_buffer << flex_file.rdbuf();
    std::string source = flex_buffer.str();

    auto def = Definition::load(source.c_str());
    if (!def || def->has_error()) {
        std::cerr << "Failed to load component_binding.flex: "
                  << (def ? def->error_message() : "null definition") << "\n";
        return 1;
    }

    auto instance = Instance::create(def);
    if (!instance || !instance->scene()) {
        std::cerr << "Failed to create instance\n";
        return 1;
    }

    instance->set_input("badgeW", 40.0f);
    instance->set_input("badgeH", 12.0f);
    instance->set_input("boxW", 30.0f);
    instance->set_input("boxH", 14.0f);
    instance->advance(0.0f);
    print_state(instance->scene(), "Initial inputs");

    instance->set_input("badgeW", 60.0f);
    instance->advance(0.0f);
    print_state(instance->scene(), "After badgeW=60");

    instance->set_input("boxW", 50.0f);
    instance->set_input("boxH", 22.0f);
    instance->advance(0.0f);
    print_state(instance->scene(), "After boxW/boxH update");

    return 0;
}
