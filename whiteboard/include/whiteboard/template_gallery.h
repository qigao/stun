#pragma once

#include "whiteboard/common.h"
#include "whiteboard/modern_canvas.h"
#include "whiteboard/template_library.h"

namespace whiteboard {

class TemplateGallery : public Window {
public:
  TemplateGallery(Widget *parent, ModernCanvas *canvas,
                  std::function<void(const Template &)> on_template_selected)
      : Window(parent, "Templates"), m_canvas(canvas), m_on_template_selected(on_template_selected),
        m_current_category("All") {
    set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 10));
    set_fixed_size(Vector2i(700, 500));
    set_modal(true);

    // Category filter buttons
    auto *category_container = new Widget(this);
    category_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 5));

    // All button
    auto *all_btn = new Button(category_container, "All");
    all_btn->set_flags(Button::RadioButton);
    all_btn->set_pushed(true);
    all_btn->set_callback([this]() {
      m_current_category = "All";
      refresh_templates();
    });
    m_category_buttons.push_back(all_btn);

    // Category buttons
    auto categories = TemplateLibrary::instance().get_categories();
    for (const auto &category : categories) {
      auto *btn = new Button(category_container, category);
      btn->set_flags(Button::RadioButton);
      btn->set_callback([this, category]() {
        m_current_category = category;
        refresh_templates();
      });
      m_category_buttons.push_back(btn);
    }

    // Scrollable template grid
    m_scroll_panel = new VScrollPanel(this);
    m_scroll_panel->set_fixed_height(380);

    m_template_container = new Widget(m_scroll_panel);
    m_template_container->set_layout(
        new GridLayout(Orientation::Horizontal, 3, Alignment::Fill, 10, 10));

    // Close button
    auto *close_btn = new Button(this, "Close");
    close_btn->set_callback([this]() { set_visible(false); });

    refresh_templates();
  }

  void refresh_templates() {
    // Clear existing template items
    while (m_template_container->child_count() > 0) {
      m_template_container->remove_child_at(0);
    }

    // Get templates for current category
    std::vector<Template> templates;
    if (m_current_category == "All") {
      templates = TemplateLibrary::instance().get_templates();
    } else {
      templates = TemplateLibrary::instance().get_templates_by_category(m_current_category);
    }

    // Create template items
    for (const auto &tmpl : templates) {
      create_template_item(tmpl);
    }

    if (parent()) {
      parent()->perform_layout(screen()->nvg_context());
    }
  }

  void create_template_item(const Template &tmpl) {
    auto *item_container = new Widget(m_template_container);
    item_container->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));
    item_container->set_fixed_size(Vector2i(200, 180));

    // Preview area (placeholder for now)
    auto *preview = new Widget(item_container);
    preview->set_fixed_size(Vector2i(190, 120));

    // Template name
    auto *name_label = new Label(item_container, tmpl.name, "sans-bold", 14);
    name_label->set_fixed_width(190);

    // Template description (on hover - using tooltip)
    auto *desc_label = new Label(item_container, tmpl.description, "sans", 11);
    desc_label->set_fixed_width(190);
    desc_label->set_color(Color(120, 120, 120, 255));

    // Use Template button
    auto *use_btn = new Button(item_container, "Use Template");
    use_btn->set_fixed_width(190);
    use_btn->set_background_color(Color(0, 120, 215, 255));
    use_btn->set_callback([this, tmpl]() {
      if (m_on_template_selected) {
        m_on_template_selected(tmpl);
      }
      set_visible(false);
    });
  }

private:
  ModernCanvas *m_canvas;
  std::function<void(const Template &)> m_on_template_selected;
  std::string m_current_category;
  VScrollPanel *m_scroll_panel;
  Widget *m_template_container;
  std::vector<Button *> m_category_buttons;
};

class ModernWhiteboardApp;
} // namespace whiteboard
