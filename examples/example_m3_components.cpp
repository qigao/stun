/*
    example_m3_components.cpp -- M3 components showcase

    Demonstrates all M3 components with proper styling and interactions.
*/

#include <nanogui/nanogui.h>
#include <nanogui/m3.h>
#include <iostream>
using namespace nanogui;

int main(int /* argc */, char ** /* argv */) {
    nanogui::init();

    {
        // Create main screen
        Screen *screen = new Screen(Vector2i(1200, 800), "M3 Components Showcase");

        // Create M3 theme with purple seed
        auto *theme = new M3Theme(
            screen->nvg_context(),
            Color(0.4f, 0.2f, 0.8f, 1.0f),  // Purple seed
            M3Theme::Scheme::Light
        );
        screen->set_theme(theme);

        // Main window
        Window *window = new Window(screen, "M3 Components");
        window->set_position(Vector2i(15, 15));
        window->set_layout(new GroupLayout());

        // Theme controls
        new Label(window, "Theme Controls", "sans-bold");
        Widget *theme_controls = new Widget(window);
        theme_controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

        Button *light_btn = new Button(theme_controls, "Light");
        light_btn->set_callback([theme, screen]() {
            theme->apply_scheme(M3Theme::Scheme::Light);
            screen->perform_layout();
        });

        Button *dark_btn = new Button(theme_controls, "Dark");
        dark_btn->set_callback([theme, screen]() {
            theme->apply_scheme(M3Theme::Scheme::Dark);
            screen->perform_layout();
        });

        // Buttons section
        new Label(window, "M3 Buttons", "sans-bold");
        Widget *buttons_panel = new Widget(window);
        buttons_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        new M3Button(buttons_panel, "Filled", 0, M3Button::Style::Filled);
        new M3Button(buttons_panel, "Outlined", 0, M3Button::Style::Outlined);
        new M3Button(buttons_panel, "Text", 0, M3Button::Style::Text);
        new M3Button(buttons_panel, "Elevated", 0, M3Button::Style::Elevated);
        new M3Button(buttons_panel, "Tonal", 0, M3Button::Style::Tonal);

        // Buttons with icons
        new Label(window, "Buttons with Icons", "sans-bold");
        Widget *icon_buttons = new Widget(window);
        icon_buttons->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        new M3Button(icon_buttons, "Add", 0xf067, M3Button::Style::Filled);
        new M3Button(icon_buttons, "Edit", 0xf044, M3Button::Style::Outlined);
        new M3Button(icon_buttons, "Delete", 0xf1f8, M3Button::Style::Text);

        // FABs section
        new Label(window, "Floating Action Buttons", "sans-bold");
        Widget *fabs_panel = new Widget(window);
        fabs_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 15, 15));

        new M3FAB(fabs_panel, 0xf067, M3FAB::Size::Small);
        new M3FAB(fabs_panel, 0xf067, M3FAB::Size::Regular);
        new M3FAB(fabs_panel, 0xf067, M3FAB::Size::Large);

        // Cards section
        new Label(window, "M3 Cards", "sans-bold");
        Widget *cards_panel = new Widget(window);
        cards_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 15, 15));

        auto *elevated_card = new M3Card(cards_panel, M3Card::Style::Elevated);
        elevated_card->set_fixed_size(Vector2i(150, 100));
        elevated_card->set_layout(new GroupLayout());
        new Label(elevated_card, "Elevated", "sans-bold");
        new Label(elevated_card, "Card with shadow", "sans");

        auto *filled_card = new M3Card(cards_panel, M3Card::Style::Filled);
        filled_card->set_fixed_size(Vector2i(150, 100));
        filled_card->set_layout(new GroupLayout());
        new Label(filled_card, "Filled", "sans-bold");
        new Label(filled_card, "Surface variant", "sans");

        auto *outlined_card = new M3Card(cards_panel, M3Card::Style::Outlined);
        outlined_card->set_fixed_size(Vector2i(150, 100));
        outlined_card->set_layout(new GroupLayout());
        new Label(outlined_card, "Outlined", "sans-bold");
        new Label(outlined_card, "With border", "sans");

        // Text Fields section
        new Label(window, "M3 Text Fields", "sans-bold");
        
        auto *filled_field = new M3TextField(window, "Hello M3", M3TextField::Style::Filled);
        filled_field->set_label("Filled Text Field");
        filled_field->set_helper_text("Helper text goes here");
        filled_field->set_editable(true);
        filled_field->set_callback([](const std::string &text) { return true; });
        filled_field->set_fixed_width(300);

        auto *outlined_field = new M3TextField(window, "Hello M3", M3TextField::Style::Outlined);
        outlined_field->set_label("Outlined Text Field");
        outlined_field->set_helper_text("Helper text goes here");
        outlined_field->set_editable(true);
        outlined_field->set_callback([](const std::string &text) { return true; });
        outlined_field->set_fixed_width(300);

        auto *error_field = new M3TextField(window, "Invalid input", M3TextField::Style::Filled);
        error_field->set_label("Error State");
        error_field->set_helper_text("This field has an error");
        error_field->set_editable(true);
        error_field->set_callback([](const std::string &text) { return true; });
        error_field->set_error(true);
        error_field->set_fixed_width(300);

        // Chips section
        new Label(window, "M3 Chips", "sans-bold");
        Widget *chips_panel = new Widget(window);
        chips_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        new M3Chip(chips_panel, "Assist", 0xf05a, M3Chip::Style::Assist);

        auto *filter_chip = new M3Chip(chips_panel, "Filter", 0xf0b0, M3Chip::Style::Filter);
        filter_chip->set_selected(true);

        auto *input_chip = new M3Chip(chips_panel, "Input", 0, M3Chip::Style::Input);
        input_chip->set_removable(true);

        new M3Chip(chips_panel, "Suggestion", 0, M3Chip::Style::Suggestion);

        // Switch section
        new Label(window, "M3 Switch", "sans-bold");
        Widget *switch_panel = new Widget(window);
        switch_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 15, 15));

        auto *switch1 = new M3Switch(switch_panel, false);
        switch1->set_callback([](bool checked) {
            std::cout << "Switch 1: " << (checked ? "ON" : "OFF") << std::endl;
        });

        auto *switch2 = new M3Switch(switch_panel, true);
        switch2->set_callback([](bool checked) {
            std::cout << "Switch 2: " << (checked ? "ON" : "OFF") << std::endl;
        });

        // Checkbox section
        new Label(window, "M3 Checkbox", "sans-bold");
        Widget *checkbox_panel = new Widget(window);
        checkbox_panel->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));

        new M3Checkbox(checkbox_panel, "Option 1", false);
        new M3Checkbox(checkbox_panel, "Option 2", true);
        new M3Checkbox(checkbox_panel, "Option 3", false);

        // Slider section
        new Label(window, "M3 Slider", "sans-bold");
        auto *slider = new M3Slider(window);
        slider->set_value(0.5f);
        slider->set_fixed_width(300);

        // Color roles showcase
        new Label(window, "Color Roles", "sans-bold");
        Widget *colors_panel = new Widget(window);
        colors_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

        Button *primary_color = new Button(colors_panel, "Primary");
        primary_color->set_background_color(theme->primary());
        primary_color->set_text_color(theme->on_primary());

        Button *secondary_color = new Button(colors_panel, "Secondary");
        secondary_color->set_background_color(theme->secondary());
        secondary_color->set_text_color(theme->on_secondary());

        Button *tertiary_color = new Button(colors_panel, "Tertiary");
        tertiary_color->set_background_color(theme->tertiary());
        tertiary_color->set_text_color(theme->on_tertiary());

        Button *error_color = new Button(colors_panel, "Error");
        error_color->set_background_color(theme->error());
        error_color->set_text_color(theme->on_error());

        // Seed color controls
        new Label(window, "Change Seed Color", "sans-bold");
        Widget *seed_controls = new Widget(window);
        seed_controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

        Button *purple_seed = new Button(seed_controls, "Purple");
        purple_seed->set_callback([theme, screen]() {
            theme->set_seed_color(Color(0.4f, 0.2f, 0.8f, 1.0f));
            screen->perform_layout();
        });

        Button *blue_seed = new Button(seed_controls, "Blue");
        blue_seed->set_callback([theme, screen]() {
            theme->set_seed_color(Color(0.2f, 0.4f, 0.9f, 1.0f));
            screen->perform_layout();
        });

        Button *green_seed = new Button(seed_controls, "Green");
        green_seed->set_callback([theme, screen]() {
            theme->set_seed_color(Color(0.2f, 0.7f, 0.3f, 1.0f));
            screen->perform_layout();
        });

        Button *red_seed = new Button(seed_controls, "Red");
        red_seed->set_callback([theme, screen]() {
            theme->set_seed_color(Color(0.8f, 0.2f, 0.2f, 1.0f));
            screen->perform_layout();
        });

        screen->set_visible(true);
        screen->perform_layout();
        screen->draw_all();

        nanogui::run();
    }

    nanogui::shutdown();
    return 0;
}
