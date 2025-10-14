/*
    examples/example_m3_rich_tooltip.cpp -- M3 Rich Tooltip Example

    This example demonstrates the M3RichTooltip component with:
    - Title and supporting text
    - Icons
    - Action buttons
    - Persistent display mode
    - Different positioning options

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <iostream>
#include <functional>
#include <utility>
#include <nanogui.h>
#include <nanogui/m3.h>

using namespace nanogui;

class HoverButton : public Button {
public:
  using Button::Button;

  void set_hover_callback(std::function<void(bool)> cb) { m_hover_callback = std::move(cb); }

  bool mouse_enter_event(const Vector2i &p, bool enter) override {
    std::cout << "HoverButton mouse_enter_event: " << (enter ? "ENTER" : "LEAVE") << std::endl;
    Button::mouse_enter_event(p, enter);
    if (m_hover_callback) {
      std::cout << "HoverButton calling hover_callback" << std::endl;
      m_hover_callback(enter);
    }
    return true;
  }

private:
  std::function<void(bool)> m_hover_callback;
};

class RichTooltipExample : public Screen {
public:
  RichTooltipExample() : Screen(Vector2i(800, 600), "M3 Rich Tooltip Example") {
    // Set M3 theme
    set_theme(new M3Theme(nvg_context()));

    // Create main window
    Window *window = new Window(this, "Rich Tooltip Examples");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());

    auto attach_tooltip = [this](HoverButton *button, auto configure) {
        ref<M3RichTooltip> tooltip = new M3RichTooltip(this);
        this->add_child(tooltip.get());
        configure(*tooltip);
        tooltip->set_target(button);

    button->set_hover_callback([tooltip](bool enter) mutable {
        std::cout << "Hover callback: " << (enter ? "SHOW_DELAYED" : "HIDE") << std::endl;
        if (enter)
            tooltip->show_delayed(100);  // Shorter delay for testing
        else
            tooltip->hide_immediate();
    });
    };

    new Label(window, "Example 1: Basic Rich Tooltip", "sans-bold");
    HoverButton *btn1 = new HoverButton(window, "Hover for info");
    attach_tooltip(btn1, [btn1](M3RichTooltip &tooltip) {
        tooltip.set_title("Feature Name");
        tooltip.set_supporting_text(
            "This is a detailed description that provides more context about the feature.");
        tooltip.set_position(M3Tooltip::Position::BOTTOM);
    });

    new Label(window, "Example 2: With Icon", "sans-bold");
    HoverButton *btn2 = new HoverButton(window, "Hover for help");
    attach_tooltip(btn2, [btn2](M3RichTooltip &tooltip) {
        tooltip.set_title("Help");
        tooltip.set_supporting_text(
            "Click the action button below to learn more about this feature.");
        tooltip.set_icon(0xf05a);
        tooltip.set_position(M3Tooltip::Position::RIGHT);
    });

    new Label(window, "Example 3: With Action", "sans-bold");
    HoverButton *btn3 = new HoverButton(window, "Hover for action");
    attach_tooltip(btn3, [btn3](M3RichTooltip &tooltip) {
        tooltip.set_title("New Feature");
        tooltip.set_supporting_text("This feature is new! Click below to learn more.");
        tooltip.add_action("Learn More",
                           []() { std::cout << "Learn More clicked!" << std::endl; });
        tooltip.set_position(M3Tooltip::Position::TOP);
    });

    new Label(window, "Example 4: Multiple Actions", "sans-bold");
    HoverButton *btn4 = new HoverButton(window, "Hover for options");
    attach_tooltip(btn4, [btn4](M3RichTooltip &tooltip) {
        tooltip.set_title("Unsaved Changes");
        tooltip.set_supporting_text("You have unsaved changes. What would you like to do?");
        tooltip.set_icon(0xf071);
        tooltip.add_action("Discard", []() { std::cout << "Changes discarded" << std::endl; });
        tooltip.add_action("Save", []() { std::cout << "Changes saved" << std::endl; });
        tooltip.set_position(M3Tooltip::Position::LEFT);
    });

    new Label(window, "Example 5: Full Featured", "sans-bold");
    HoverButton *btn5 = new HoverButton(window, "Hover for full example");
    attach_tooltip(btn5, [btn5](M3RichTooltip &tooltip) {
        tooltip.set_title("Premium Feature");
        tooltip.set_supporting_text("This is a premium feature. Upgrade your account to unlock it "
                                    "and get access to many more features.");
        tooltip.set_icon(0xf005);
        tooltip.add_action("Not Now", []() { std::cout << "Dismissed" << std::endl; });
        tooltip.add_action("Upgrade", []() { std::cout << "Upgrade clicked!" << std::endl; });
        tooltip.set_position(M3Tooltip::Position::AUTO);
    });

    new Label(window, "Example 6: Persistent Display", "sans-bold");
    new Label(window, "(Rich tooltips stay visible until dismissed)", "sans", 12);
    HoverButton *btn6 = new HoverButton(window, "Show persistent tooltip");
    btn6->set_callback([this, btn6]() {
        ref<M3RichTooltip> tooltip = new M3RichTooltip(this);
        this->add_child(tooltip.get());
        tooltip->set_title("Persistent Tooltip");
        tooltip->set_supporting_text(
            "This tooltip will remain visible until you click an action or click outside.");
        tooltip->add_action("Got it", []() { std::cout << "Tooltip dismissed" << std::endl; });
        tooltip->set_target(btn6);
        tooltip->set_persistent(true);
        tooltip->show_immediate();
    });

    new Label(window, "", "sans");
    new Label(window, "Instructions:", "sans-bold");
    new Label(window, "- Hover over buttons to see tooltips", "sans", 12);
    new Label(window, "- Rich tooltips are persistent", "sans", 12);
    new Label(window, "- Click actions to dismiss", "sans", 12);
    new Label(window, "- Click outside to dismiss", "sans", 12);

    perform_layout();
    window->center();
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;
    // ESC key to close (key code 256 for GLFW, 27 for SDL)
    if ((key == 256 || key == 27) && action == 1) {
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
      nanogui::ref<RichTooltipExample> app = new RichTooltipExample();
      app->set_visible(true);
      app->perform_layout();
      app->draw_all();
      nanogui::run();
    }

    nanogui::shutdown();
  } catch (const std::runtime_error &e) {
    std::string error_msg = std::string("Caught a fatal error: ") + std::string(e.what());
    std::cerr << error_msg << std::endl;
    return -1;
  }

  return 0;
}
