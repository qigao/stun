/*
    examples/test_animations.cpp -- Test animation system for M3 components

    This example demonstrates the animation system for M3Dialog, M3Menu, and M3Tooltip.
    It also shows how to disable animations for accessibility.
*/

#include <iostream>
#include <nanogui/button.h>
#include <nanogui/checkbox.h>
#include <nanogui/label.h>
#include <nanogui/m3_dialog.h>
#include <nanogui/m3_menu.h>
#include <nanogui/m3_theme.h>
#include <nanogui/m3_tooltip.h>
#include <nanogui.h>

using namespace nanogui;

class HoverButton : public Button {
public:
  using Button::Button;

  void set_hover_callback(std::function<void(bool)> cb) { m_hover_callback = std::move(cb); }

  bool mouse_enter_event(const Vector2i &p, bool enter) override {
    Button::mouse_enter_event(p, enter);
    if (m_hover_callback) {
      m_hover_callback(enter);
    }
    return true;
  }

private:
  std::function<void(bool)> m_hover_callback;
};

class AnimationTestApp : public Screen {
public:
  AnimationTestApp() : Screen(Vector2i(800, 600), "M3 Animation System Test") {
    // Create M3 theme
    M3Theme *theme = new M3Theme(nvg_context());
    set_theme(theme);

    // Main container
    Widget *container = new Widget(this);
    container->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Middle, 20, 20));

    // Title
    new Label(container, "M3 Animation System Test", "sans-bold", 24);

    // Animation toggle
    Widget *toggle_panel = new Widget(container);
    toggle_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 10));

    new Label(toggle_panel, "Enable Animations:", "sans", 16);
    CheckBox *animation_toggle = new CheckBox(toggle_panel, "");
    animation_toggle->set_checked(true);
    animation_toggle->set_callback(
        [theme](bool checked) { theme->set_animations_enabled(checked); });

    // Dialog test buttons
    new Label(container, "Dialog Animations", "sans-bold", 18);

    Button *basic_dialog_btn = new Button(container, "Show Basic Dialog");
    basic_dialog_btn->set_callback([this]() {
      M3Dialog *dialog = new M3Dialog(this, "Basic Dialog", M3Dialog::DialogType::BASIC);
      dialog->set_content_text("This dialog animates with scale and fade effects.\n\n"
                               "Show: 200ms scale from 0.8 to 1.0\n"
                               "Hide: 150ms scale from 1.0 to 0.8");
      dialog->add_action("Cancel", [dialog]() { dialog->hide(); });
      dialog->add_action("OK", [dialog]() { dialog->hide(); });
      dialog->show();
    });

    Button *fullscreen_dialog_btn = new Button(container, "Show Fullscreen Dialog");
    fullscreen_dialog_btn->set_callback([this]() {
      M3Dialog *dialog = new M3Dialog(this, "Fullscreen Dialog", M3Dialog::DialogType::FULLSCREEN);
      dialog->set_content_text("This dialog slides in from the right.\n\n"
                               "Show: 300ms slide from right\n"
                               "Hide: 250ms slide to right");
      dialog->add_action("Close", [dialog]() { dialog->hide(); });
      dialog->show();
    });

    // Menu test button
    new Label(container, "Menu Animations", "sans-bold", 18);

    Button *menu_btn = new Button(container, "Show Menu");
    menu_btn->set_callback([this, menu_btn]() {
      M3Menu *menu = new M3Menu(this, menu_btn);
      menu->add_item("Menu Item 1", []() {});
      menu->add_item("Menu Item 2", []() {});
      menu->add_item("Menu Item 3", []() {});
      menu->add_divider();
      menu->add_item("Close", [menu]() { menu->set_visible(false); });
      menu->show_at(menu_btn);
    });

    // Tooltip test
    new Label(container, "Tooltip Animations", "sans-bold", 18);

    HoverButton *tooltip_btn = new HoverButton(container, "Hover for Tooltip");

    M3Tooltip *tooltip = new M3Tooltip(this, "This tooltip fades in after 500ms delay.\n"
                                             "Fade-in: 150ms\n"
                                             "Fade-out: 75ms");
    this->add_child(tooltip);
    tooltip->set_target(tooltip_btn);

    // Proper hover behavior for tooltip
    tooltip_btn->set_hover_callback([tooltip](bool enter) {
        if (enter) {
            tooltip->show_delayed(500);
        } else {
            tooltip->hide_immediate();
        }
    });

    // Instructions
    new Label(container,
              "Toggle animations on/off to see the difference.\n"
              "When disabled, all components show/hide instantly.",
              "sans", 14);

    perform_layout();
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;
    if (key == 27 && action == 1) {
      set_visible(false);
      return true;
    }
    return false;
  }
};

int main(int argc, char **argv) {
  try {
    nanogui::init();
    {
      nanogui::ref<AnimationTestApp> app = new AnimationTestApp();
      app->set_visible(true);
      app->draw_all();
      nanogui::run();
    }
    nanogui::shutdown();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}
