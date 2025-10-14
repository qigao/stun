#pragma once

#include "whiteboard/common.h"
#include "whiteboard/modern_canvas.h"

namespace whiteboard {

class LayerItem : public Widget {
public:
  LayerItem(Widget *parent, int stroke_index, const Stroke &stroke,
            std::function<void(int)> on_select, std::function<void(int)> on_visibility_toggle,
            std::function<void(int)> on_lock_toggle)
      : Widget(parent), m_stroke_index(stroke_index), m_on_select(on_select),
        m_on_visibility_toggle(on_visibility_toggle), m_on_lock_toggle(on_lock_toggle) {

    set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 5));
    set_fixed_height(35);

    // Icon for stroke type
    std::string icon_text = get_icon_for_tool(stroke.tool);
    auto *icon_label = new Label(this, icon_text, "icons");
    icon_label->set_fixed_width(25);
    icon_label->set_font_size(16);

    // Name/type label
    std::string display_name = get_display_name(stroke);
    auto *name_label = new Label(this, display_name, "sans");
    name_label->set_fixed_width(120);
    name_label->set_font_size(12);
    m_name_label = name_label;

    // Visibility toggle button
    auto *vis_btn = new Button(this, "", stroke.visible ? FA_EYE : FA_EYE_SLASH);
    vis_btn->set_fixed_size(Vector2i(25, 25));
    vis_btn->set_tooltip(stroke.visible ? "Hide" : "Show");
    vis_btn->set_callback([this]() {
      if (m_on_visibility_toggle) {
        m_on_visibility_toggle(m_stroke_index);
      }
    });
    m_visibility_button = vis_btn;

    // Lock toggle button
    auto *lock_btn = new Button(this, "", stroke.locked ? FA_LOCK : FA_UNLOCK);
    lock_btn->set_fixed_size(Vector2i(25, 25));
    lock_btn->set_tooltip(stroke.locked ? "Unlock" : "Lock");
    lock_btn->set_callback([this]() {
      if (m_on_lock_toggle) {
        m_on_lock_toggle(m_stroke_index);
      }
    });
    m_lock_button = lock_btn;
  }

  virtual bool mouse_button_event(const Vector2i &p, int button, bool down,
                                  int modifiers) override {
    if (Widget::mouse_button_event(p, button, down, modifiers))
      return true;

    if (button == NANOGUI_MOUSE_BUTTON_LEFT && down) {
      if (m_on_select) {
        m_on_select(m_stroke_index);
      }
      return true;
    }
    return false;
  }

  void update_visibility(bool visible) {
    if (m_visibility_button) {
      m_visibility_button->set_icon(visible ? FA_EYE : FA_EYE_SLASH);
      m_visibility_button->set_tooltip(visible ? "Hide" : "Show");
    }
  }

  void update_lock(bool locked) {
    if (m_lock_button) {
      m_lock_button->set_icon(locked ? FA_LOCK : FA_UNLOCK);
      m_lock_button->set_tooltip(locked ? "Unlock" : "Lock");
    }
  }

private:
  int m_stroke_index;
  std::function<void(int)> m_on_select;
  std::function<void(int)> m_on_visibility_toggle;
  std::function<void(int)> m_on_lock_toggle;
  Label *m_name_label = nullptr;
  Button *m_visibility_button = nullptr;
  Button *m_lock_button = nullptr;

  std::string get_icon_for_tool(Tool tool) {
    switch (tool) {
    case Tool::Pen:
      return std::string(1, (char)FA_PEN);
    case Tool::Text:
      return std::string(1, (char)FA_FONT);
    case Tool::Sticky:
      return std::string(1, (char)FA_STICKY_NOTE);
    case Tool::Rectangle:
      return std::string(1, (char)FA_SQUARE);
    case Tool::Circle:
      return std::string(1, (char)FA_CIRCLE);
    case Tool::Line:
      return std::string(1, (char)FA_MINUS);
    case Tool::Arrow:
      return std::string(1, (char)FA_ARROW_RIGHT);
    case Tool::Image:
      return std::string(1, (char)FA_IMAGE);
    default:
      return std::string(1, (char)FA_QUESTION);
    }
  }

  std::string get_display_name(const Stroke &stroke) {
    if (!stroke.name.empty()) {
      return stroke.name;
    }

    // Generate default name based on tool type
    switch (stroke.tool) {
    case Tool::Pen:
      return "Pen Stroke";
    case Tool::Text:
      if (!stroke.text.empty()) {
        std::string preview = stroke.text.substr(0, 15);
        if (stroke.text.length() > 15)
          preview += "...";
        return "Text: " + preview;
      }
      return "Text";
    case Tool::Sticky:
      return "Sticky Note";
    case Tool::Rectangle:
      return "Rectangle";
    case Tool::Circle:
      return "Circle";
    case Tool::Line:
      return "Line";
    case Tool::Arrow:
      return "Arrow";
    case Tool::Image:
      if (!stroke.file_path.empty()) {
        // Extract filename from path
        size_t last_slash = stroke.file_path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos)
                                   ? stroke.file_path.substr(last_slash + 1)
                                   : stroke.file_path;
        return "Image: " + filename;
      }
      return "Image";
    default:
      return "Shape";
    }
  }
};

// LayersPanel class for managing layers
class LayersPanel : public Window {
public:
  LayersPanel(Widget *parent, ModernCanvas *canvas) : Window(parent, "Layers"), m_canvas(canvas) {
    set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));
    set_fixed_width(220);
    set_fixed_height(400);

    m_scroll_panel = new VScrollPanel(this);
    m_scroll_panel->set_fixed_height(350);

    m_layer_container = new Widget(m_scroll_panel);
    m_layer_container->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 2, 2));

    refresh();
  }

  void refresh() {
    if (!m_canvas)
      return;

    while (m_layer_container->child_count() > 0) {
      m_layer_container->remove_child_at(0);
    }

    const auto strokes = m_canvas->get_strokes();
    for (int i = static_cast<int>(strokes.size()) - 1; i >= 0; --i) {
      const auto &stroke = strokes[static_cast<size_t>(i)];
      new LayerItem(
          m_layer_container, i, stroke, [this](int idx) { on_layer_selected(idx); },
          [this](int idx) { on_visibility_toggled(idx); },
          [this](int idx) { on_lock_toggled(idx); });
    }

    if (parent()) {
      parent()->perform_layout(screen()->nvg_context());
    }
  }

private:
  void on_layer_selected(int index) {
    if (!m_canvas)
      return;
    m_canvas->select_layer(index);
    refresh();
  }

  void on_visibility_toggled(int index) {
    if (!m_canvas)
      return;
    m_canvas->toggle_layer_visibility(index);
    refresh();
  }

  void on_lock_toggled(int index) {
    if (!m_canvas)
      return;
    m_canvas->toggle_layer_lock(index);
    refresh();
  }

  ModernCanvas *m_canvas = nullptr;
  VScrollPanel *m_scroll_panel = nullptr;
  Widget *m_layer_container = nullptr;
};

} // namespace whiteboard
