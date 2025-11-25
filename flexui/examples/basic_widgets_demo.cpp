#include <flexui/screen.h>
#include <flexui/button.h>
#include <flexui/checkbox.h>
#include <flexui/textbox.h>
#include <iostream>

int main() {
    flexui::Screen screen(800, 600, "Basic Widgets Demo");

    // Load CSS for layout - Flexbox column (matching example_layouts.cpp pattern)
    screen.loadCSS(R"(
        #container {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 20px;
            width: 300px;
            height: 400px;
            background: #f0f0f0;
            border: 2px solid #333;
            border-radius: 8px;
            position: absolute;
            top: 20px;
            left: 20px;
        }
        button {
            width: 160px;
            height: 48px;
        }
        checkbox {
            width: 32px;
            height: 32px;
        }
        input {
            width: 250px;
            height: 48px;
        }
    )");

    // Create container
    auto* container = screen.addWidget("container", "div");

    // Create Button
    auto* btn = screen.createWidget<flexui::Button>("btn1", "Click Me");
    btn->setClickCallback([](flexui::Widget* w) {
        std::cout << "Button clicked!" << std::endl;
        return true;
    });
    container->addChild(btn);

    // Create Checkbox
    auto* checkbox = screen.createWidget<flexui::Checkbox>("chk1", false);
    checkbox->setChangeCallback([](bool checked) {
        std::cout << "Checkbox: " << (checked ? "checked" : "unchecked") << std::endl;
    });
    container->addChild(checkbox);

    // Create TextBox
    auto* textbox = screen.createWidget<flexui::TextBox>("txt1", "Enter text...");
    textbox->setScreen(&screen);
    textbox->setChangeCallback([](const std::string& text) {
        std::cout << "TextBox: " << text << std::endl;
    });
    container->addChild(textbox);

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
