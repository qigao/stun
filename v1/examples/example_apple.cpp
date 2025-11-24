/*
    examples/example_apple.cpp -- Apple Human Interface Guidelines theme demo

    Demonstrates Apple HIG components with light/dark mode support.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <iostream>
#include <nanogui.h>
#include <nanogui/apple.h>

using namespace nanogui;

class AppleExampleApp : public Screen {
public:
  AppleExampleApp() : Screen(Vector2i(800, 600), "Apple HIG Theme Demo") {
    // Create Apple theme
    m_apple_theme = new AppleTheme(m_nvg_context);
    m_apple_theme->set_accent_color(AppleTheme::AccentColor::Blue);
    m_apple_theme->apply_appearance(AppleTheme::Appearance::Light);

    // Main window
    Window *window = new Window(this, "Apple Components");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());
    window->set_theme(m_apple_theme);

    // Title
    new Label(window, "Apple Human Interface Guidelines", "sans-bold", 24);
    new Label(window, "Native macOS/iOS styling", "sans", 16);

    // Buttons section
    new Label(window, "Buttons", "sans-bold", 18);

    Widget *button_row = new Widget(window);
    button_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));

    auto primary_btn = new AppleButton(button_row, "Primary", 0, AppleButton::Style::Primary);
    primary_btn->set_callback([]() { std::cout << "Primary button clicked" << std::endl; });

    auto secondary_btn = new AppleButton(button_row, "Secondary", 0, AppleButton::Style::Secondary);
    secondary_btn->set_callback([]() { std::cout << "Secondary button clicked" << std::endl; });

    auto tertiary_btn = new AppleButton(button_row, "Tertiary", 0, AppleButton::Style::Tertiary);
    tertiary_btn->set_callback([]() { std::cout << "Tertiary button clicked" << std::endl; });

    auto destructive_btn =
        new AppleButton(button_row, "Delete", 0, AppleButton::Style::Destructive);
    destructive_btn->set_callback([]() { std::cout << "Destructive button clicked" << std::endl; });

    // Toggle section
    new Label(window, "Toggle Switch", "sans-bold", 18);

    Widget *toggle_row = new Widget(window);
    toggle_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));

    new Label(toggle_row, "Enable notifications");
    auto toggle = new AppleToggle(toggle_row, "");
    toggle->set_callback(
        [](bool state) { std::cout << "Toggle: " << (state ? "ON" : "OFF") << std::endl; });

    // Checkbox section
    new Label(window, "Checkbox", "sans-bold", 18);
    auto checkbox = new AppleCheckbox(window, "I agree to the terms");
    checkbox->set_callback([](bool checked) {
      std::cout << "Checkbox: " << (checked ? "Checked" : "Unchecked") << std::endl;
    });

    // Slider section
    new Label(window, "Slider", "sans-bold", 18);
    auto slider = new AppleSlider(window);
    slider->set_value(0.5f);
    slider->set_callback([](float value) { std::cout << "Slider: " << value << std::endl; });

    // Progress section
    new Label(window, "Progress", "sans-bold", 18);
    auto progress = new AppleProgress(window);
    progress->set_value(0.7f);

    // Segmented Control section
    new Label(window, "Segmented Control", "sans-bold", 18);
    auto segmented = new AppleSegmentedControl(window, {"Day", "Week", "Month"});
    segmented->set_callback(
        [](int index) { std::cout << "Selected segment: " << index << std::endl; });

    // Badge section
    new Label(window, "Badge", "sans-bold", 18);
    Widget *badge_row = new Widget(window);
    badge_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 12));

    auto badge1 = new AppleBadge(badge_row, "");
    badge1->set_count(5);

    auto badge2 = new AppleBadge(badge_row, "");
    badge2->set_count(99);

    auto badge3 = new AppleBadge(badge_row, "");
    badge3->set_count(150);

    // Stepper section
    new Label(window, "Stepper", "sans-bold", 18);
    Widget *stepper_row = new Widget(window);
    stepper_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 12));

    new Label(stepper_row, "Quantity:");
    auto stepper = new AppleStepper(stepper_row);
    stepper->set_value(5);
    stepper->set_min_value(0);
    stepper->set_max_value(10);
    stepper->set_callback([](int value) { std::cout << "Stepper value: " << value << std::endl; });

    // List section
    new Label(window, "List", "sans-bold", 18);
    auto list = new AppleList(window, AppleList::Style::Grouped);
    list->add_item("Home", "", FA_HOME, true);
    list->add_item("Settings", "", FA_COG, true);
    list->add_item("Profile", "", FA_USER, true);
    list->set_callback(
        [](int index) { std::cout << "List item selected: " << index << std::endl; });

    // Appearance switcher
    new Label(window, "Appearance", "sans-bold", 18);

    Widget *appearance_row = new Widget(window);
    appearance_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));

    auto light_btn = new AppleButton(appearance_row, "Light", 0, AppleButton::Style::Secondary);
    light_btn->set_callback([this]() {
      m_apple_theme->apply_appearance(AppleTheme::Appearance::Light);
      std::cout << "Switched to light mode" << std::endl;
    });

    auto dark_btn = new AppleButton(appearance_row, "Dark", 0, AppleButton::Style::Secondary);
    dark_btn->set_callback([this]() {
      m_apple_theme->apply_appearance(AppleTheme::Appearance::Dark);
      std::cout << "Switched to dark mode" << std::endl;
    });

    // Accent color section
    new Label(window, "Accent Color", "sans-bold", 18);

    Widget *accent_row = new Widget(window);
    accent_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));

    auto blue_btn = new AppleButton(accent_row, "Blue", 0, AppleButton::Style::Tertiary);
    blue_btn->set_callback([this]() {
      m_apple_theme->set_accent_color(AppleTheme::AccentColor::Blue);
      std::cout << "Accent: Blue" << std::endl;
    });

    auto purple_btn = new AppleButton(accent_row, "Purple", 0, AppleButton::Style::Tertiary);
    purple_btn->set_callback([this]() {
      m_apple_theme->set_accent_color(AppleTheme::AccentColor::Purple);
      std::cout << "Accent: Purple" << std::endl;
    });

    auto green_btn = new AppleButton(accent_row, "Green", 0, AppleButton::Style::Tertiary);
    green_btn->set_callback([this]() {
      m_apple_theme->set_accent_color(AppleTheme::AccentColor::Green);
      std::cout << "Accent: Green" << std::endl;
    });

    auto red_btn = new AppleButton(accent_row, "Red", 0, AppleButton::Style::Tertiary);
    red_btn->set_callback([this]() {
      m_apple_theme->set_accent_color(AppleTheme::AccentColor::Red);
      std::cout << "Accent: Red" << std::endl;
    });

    perform_layout();
  }

private:
  AppleTheme *m_apple_theme;
};

int main(int /* argc */, char ** /* argv */) {
  try {
    nanogui::init();

    {
      nanogui::ref<AppleExampleApp> app = new AppleExampleApp();
      app->draw_all();
      app->set_visible(true);
      nanogui::run();
    }

    nanogui::shutdown();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}
