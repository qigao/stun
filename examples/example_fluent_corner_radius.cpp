/*
    examples/example_fluent_corner_radius.cpp -- Demonstrates Fluent Design corner radius API

    This example shows how to use the corner_radius() method in FluentTheme
    to get consistent corner radii across different UI elements.
*/

#include <nanogui/nanogui.h>
#include <nanogui/fluent_theme.h>

using namespace nanogui;

int main() {
    nanogui::init();

    {
        Screen *screen = new Screen(Vector2i(800, 600), "Fluent Corner Radius Demo");
        
        // Create Fluent theme
        auto theme = new FluentTheme(screen->nvg_context(), FluentTheme::Palette::Light);
        screen->set_theme(theme);

        Window *window = new Window(screen, "Corner Radius Examples");
        window->set_position(Vector2i(15, 15));
        window->set_layout(new GroupLayout());

        // Demonstrate different corner radius styles
        new Label(window, "Fluent Design Corner Radius Styles", "sans-bold");
        
        Widget *container = new Widget(window);
        container->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 10));

        // None
        auto none_radius = theme->corner_radius(FluentTheme::CornerRadius::None);
        new Label(container, "None: " + std::to_string(none_radius) + "px");
        
        // Small
        auto small_radius = theme->corner_radius(FluentTheme::CornerRadius::Small);
        new Label(container, "Small: " + std::to_string(small_radius) + "px (Subtle rounding)");
        
        // Medium
        auto medium_radius = theme->corner_radius(FluentTheme::CornerRadius::Medium);
        new Label(container, "Medium: " + std::to_string(medium_radius) + "px (Standard controls)");
        
        // Large
        auto large_radius = theme->corner_radius(FluentTheme::CornerRadius::Large);
        new Label(container, "Large: " + std::to_string(large_radius) + "px (Cards, dialogs)");
        
        // ExtraLarge
        auto xlarge_radius = theme->corner_radius(FluentTheme::CornerRadius::ExtraLarge);
        new Label(container, "ExtraLarge: " + std::to_string(xlarge_radius) + "px (Large surfaces)");
        
        // Circle
        new Label(container, "Circle: 9999px (Pill shape)");

        screen->set_visible(true);
        screen->perform_layout();

        nanogui::mainloop();
    }

    nanogui::shutdown();
    return 0;
}
