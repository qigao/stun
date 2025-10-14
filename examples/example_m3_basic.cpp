/*
    example_m3_basic.cpp -- Basic Material Design 3 theme example

    Demonstrates the M3 theme with dynamic color generation from a seed color.
*/

#include <nanogui.h>
#include <nanogui/m3_theme.h>

using namespace nanogui;

int main(int /* argc */, char ** /* argv */) {
  nanogui::init();

  {
    // Create main screen
    Screen *screen = new Screen(Vector2i(800, 600), "Material Design 3 Theme Demo");

    // Create M3 theme with purple seed color
    auto *m3_theme =
        new M3Theme(screen->nvg_context(), Color(0.4f, 0.2f, 0.8f, 1.0f), // Purple seed
                    M3Theme::Scheme::Light);
    screen->set_theme(m3_theme);

    // Create main window
    Window *window = new Window(screen, "M3 Theme Showcase");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());

    // Color roles section
    new Label(window, "M3 Color Roles", "sans-bold");

    Widget *color_panel = new Widget(window);
    color_panel->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 10));

    // Primary colors
    Widget *primary_row = new Widget(color_panel);
    primary_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

    Button *primary_btn = new Button(primary_row, "Primary");
    primary_btn->set_background_color(m3_theme->primary());
    primary_btn->set_text_color(m3_theme->on_primary());

    Button *primary_container_btn = new Button(primary_row, "Primary Container");
    primary_container_btn->set_background_color(m3_theme->primary_container());
    primary_container_btn->set_text_color(m3_theme->on_primary_container());

    // Secondary colors
    Widget *secondary_row = new Widget(color_panel);
    secondary_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

    Button *secondary_btn = new Button(secondary_row, "Secondary");
    secondary_btn->set_background_color(m3_theme->secondary());
    secondary_btn->set_text_color(m3_theme->on_secondary());

    Button *secondary_container_btn = new Button(secondary_row, "Secondary Container");
    secondary_container_btn->set_background_color(m3_theme->secondary_container());
    secondary_container_btn->set_text_color(m3_theme->on_secondary_container());

    // Tertiary colors
    Widget *tertiary_row = new Widget(color_panel);
    tertiary_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

    Button *tertiary_btn = new Button(tertiary_row, "Tertiary");
    tertiary_btn->set_background_color(m3_theme->tertiary());
    tertiary_btn->set_text_color(m3_theme->on_tertiary());

    Button *tertiary_container_btn = new Button(tertiary_row, "Tertiary Container");
    tertiary_container_btn->set_background_color(m3_theme->tertiary_container());
    tertiary_container_btn->set_text_color(m3_theme->on_tertiary_container());

    // Error colors
    Widget *error_row = new Widget(color_panel);
    error_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

    Button *error_btn = new Button(error_row, "Error");
    error_btn->set_background_color(m3_theme->error());
    error_btn->set_text_color(m3_theme->on_error());

    Button *error_container_btn = new Button(error_row, "Error Container");
    error_container_btn->set_background_color(m3_theme->error_container());
    error_container_btn->set_text_color(m3_theme->on_error_container());

    // Theme switcher
    new Label(window, "Theme Controls", "sans-bold");

    Widget *controls = new Widget(window);
    controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

    Button *light_btn = new Button(controls, "Light Mode");
    light_btn->set_callback([m3_theme, screen]() {
      m3_theme->apply_scheme(M3Theme::Scheme::Light);
      screen->perform_layout();
    });

    Button *dark_btn = new Button(controls, "Dark Mode");
    dark_btn->set_callback([m3_theme, screen]() {
      m3_theme->apply_scheme(M3Theme::Scheme::Dark);
      screen->perform_layout();
    });

    // Seed color examples
    new Label(window, "Try Different Seed Colors", "sans-bold");

    Widget *seed_controls = new Widget(window);
    seed_controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

    Button *purple_seed = new Button(seed_controls, "Purple");
    purple_seed->set_callback([m3_theme, screen]() {
      m3_theme->set_seed_color(Color(0.4f, 0.2f, 0.8f, 1.0f));
      screen->perform_layout();
    });

    Button *blue_seed = new Button(seed_controls, "Blue");
    blue_seed->set_callback([m3_theme, screen]() {
      m3_theme->set_seed_color(Color(0.2f, 0.4f, 0.9f, 1.0f));
      screen->perform_layout();
    });

    Button *green_seed = new Button(seed_controls, "Green");
    green_seed->set_callback([m3_theme, screen]() {
      m3_theme->set_seed_color(Color(0.2f, 0.7f, 0.3f, 1.0f));
      screen->perform_layout();
    });

    Button *red_seed = new Button(seed_controls, "Red");
    red_seed->set_callback([m3_theme, screen]() {
      m3_theme->set_seed_color(Color(0.8f, 0.2f, 0.2f, 1.0f));
      screen->perform_layout();
    });

    // Info
    new Label(window, "M3 Theme Features:", "sans-bold");
    new Label(window, "• Dynamic color from seed", "sans");
    new Label(window, "• 13 color roles per scheme", "sans");
    new Label(window, "• Tonal palettes", "sans");
    new Label(window, "• Light & dark modes", "sans");

    screen->set_visible(true);
    screen->perform_layout();
    screen->draw_all();

    nanogui::run();
  }

  nanogui::shutdown();
  return 0;
}
