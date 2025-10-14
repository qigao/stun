#pragma once

#include "whiteboard/common.h"
#include "whiteboard/modern_canvas.h"
#include <vector>
namespace whiteboard {

class PropertiesPanel : public Window {
public:
  PropertiesPanel(Widget *parent, ModernCanvas *canvas)
      : Window(parent, "Properties"), m_canvas(canvas) {
    set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 10));
    set_fixed_width(220);

    // Stroke Color Section
    auto *stroke_label = new Label(this, "Stroke Color", "sans-bold", 14);
    stroke_label->set_color(Color(100, 100, 100, 255));

    m_stroke_color_picker = new ColorPicker(this, Color(255, 100, 100, 255));
    m_stroke_color_picker->set_fixed_size(Vector2i(200, 30));
    m_stroke_color_picker->set_final_callback(
        [this](const Color &color) { on_stroke_color_changed(color); });

    // Fill Color Section
    auto *fill_label = new Label(this, "Fill Color", "sans-bold", 14);
    fill_label->set_color(Color(100, 100, 100, 255));

    auto *fill_container = new Widget(this);
    fill_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 0));

    m_fill_color_picker = new ColorPicker(fill_container, Color(255, 182, 193, 255));
    m_fill_color_picker->set_fixed_size(Vector2i(150, 30));
    m_fill_color_picker->set_final_callback(
        [this](const Color &color) { on_fill_color_changed(color); });

    m_fill_none_checkbox = new CheckBox(fill_container, "None");
    m_fill_none_checkbox->set_callback([this](bool checked) { on_fill_none_changed(checked); });

    // Stroke Width Section
    auto *width_label = new Label(this, "Stroke Width", "sans-bold", 14);
    width_label->set_color(Color(100, 100, 100, 255));

    auto *width_container = new Widget(this);
    width_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 0));

    m_stroke_width_slider = new Slider(width_container);
    m_stroke_width_slider->set_range({1.0f, 20.0f});
    m_stroke_width_slider->set_value(3.0f);
    m_stroke_width_slider->set_fixed_width(150);
    m_stroke_width_slider->set_callback([this](float value) {
      on_stroke_width_changed(value);
      if (m_width_textbox) {
        m_width_textbox->set_value(std::to_string((int)value));
      }
    });

    m_width_textbox = new TextBox(width_container, "3");
    m_width_textbox->set_fixed_size(Vector2i(45, 25));
    m_width_textbox->set_editable(true);
    m_width_textbox->set_alignment(TextBox::Alignment::Right);
    m_width_textbox->set_callback([this](const std::string &value) {
      try {
        float width = std::stof(value);
        width = nanogui::clip(width, 1.0f, 20.0f);
        m_stroke_width_slider->set_value(width);
        on_stroke_width_changed(width);
      } catch (...) {
      }
      return true;
    });

    // Opacity Section
    auto *opacity_label = new Label(this, "Opacity", "sans-bold", 14);
    opacity_label->set_color(Color(100, 100, 100, 255));

    auto *opacity_container = new Widget(this);
    opacity_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 0));

    m_opacity_slider = new Slider(opacity_container);
    m_opacity_slider->set_range({0.0f, 100.0f});
    m_opacity_slider->set_value(100.0f);
    m_opacity_slider->set_fixed_width(150);
    m_opacity_slider->set_callback([this](float value) {
      on_opacity_changed(value / 100.0f);
      if (m_opacity_textbox) {
        m_opacity_textbox->set_value(std::to_string((int)value));
      }
    });

    m_opacity_textbox = new TextBox(opacity_container, "100");
    m_opacity_textbox->set_fixed_size(Vector2i(45, 25));
    m_opacity_textbox->set_editable(true);
    m_opacity_textbox->set_alignment(TextBox::Alignment::Right);
    m_opacity_textbox->set_callback([this](const std::string &value) {
      try {
        float opacity = std::stof(value);
        opacity = nanogui::clip(opacity, 0.0f, 100.0f);
        m_opacity_slider->set_value(opacity);
        on_opacity_changed(opacity / 100.0f);
      } catch (...) {
      }
      return true;
    });

    // Rotation Section
    auto *rotation_label = new Label(this, "Rotation (degrees)", "sans-bold", 14);
    rotation_label->set_color(Color(100, 100, 100, 255));

    m_rotation_textbox = new TextBox(this, "0");
    m_rotation_textbox->set_fixed_size(Vector2i(200, 25));
    m_rotation_textbox->set_editable(true);
    m_rotation_textbox->set_alignment(TextBox::Alignment::Left);
    m_rotation_textbox->set_callback([this](const std::string &value) {
      try {
        float degrees = std::stof(value);
        // Normalize to 0-360 range
        while (degrees < 0)
          degrees += 360.0f;
        while (degrees >= 360.0f)
          degrees -= 360.0f;
        on_rotation_changed(degrees * M_PI / 180.0f);
      } catch (...) {
      }
      return true;
    });

    // Position Section
    auto *position_label = new Label(this, "Position", "sans-bold", 14);
    position_label->set_color(Color(100, 100, 100, 255));

    auto *pos_container = new Widget(this);
    pos_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 0));

    new Label(pos_container, "X:", "sans", 12);
    m_position_x_textbox = new TextBox(pos_container, "0");
    m_position_x_textbox->set_fixed_size(Vector2i(70, 25));
    m_position_x_textbox->set_editable(true);
    m_position_x_textbox->set_callback([this](const std::string &value) {
      try {
        float x = std::stof(value);
        on_position_changed(x, -1.0f); // -1 means don't change Y
      } catch (...) {
      }
      return true;
    });

    new Label(pos_container, "Y:", "sans", 12);
    m_position_y_textbox = new TextBox(pos_container, "0");
    m_position_y_textbox->set_fixed_size(Vector2i(70, 25));
    m_position_y_textbox->set_editable(true);
    m_position_y_textbox->set_callback([this](const std::string &value) {
      try {
        float y = std::stof(value);
        on_position_changed(-1.0f, y); // -1 means don't change X
      } catch (...) {
      }
      return true;
    });

    // Size Section (for rectangles and images)
    auto *size_label = new Label(this, "Size", "sans-bold", 14);
    size_label->set_color(Color(100, 100, 100, 255));

    auto *size_container = new Widget(this);
    size_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 0));

    new Label(size_container, "W:", "sans", 12);
    m_size_width_textbox = new TextBox(size_container, "0");
    m_size_width_textbox->set_fixed_size(Vector2i(70, 25));
    m_size_width_textbox->set_editable(true);
    m_size_width_textbox->set_callback([this](const std::string &value) {
      try {
        float w = std::stof(value);
        on_size_changed(w, -1.0f); // -1 means don't change height
      } catch (...) {
      }
      return true;
    });

    new Label(size_container, "H:", "sans", 12);
    m_size_height_textbox = new TextBox(size_container, "0");
    m_size_height_textbox->set_fixed_size(Vector2i(70, 25));
    m_size_height_textbox->set_editable(true);
    m_size_height_textbox->set_callback([this](const std::string &value) {
      try {
        float h = std::stof(value);
        on_size_changed(-1.0f, h); // -1 means don't change width
      } catch (...) {
      }
      return true;
    });
  }

  void update_for_selection(const std::vector<int> &selected_indices,
                            const std::vector<Stroke> &strokes) {
    m_updating = true; // Prevent callbacks during update

    if (selected_indices.empty()) {
      // Show default tool settings
      set_title("Properties");
      m_stroke_color_picker->set_color(Color(255, 100, 100, 255));
      m_fill_color_picker->set_color(Color(255, 182, 193, 255));
      m_stroke_width_slider->set_value(3.0f);
      m_width_textbox->set_value("3");
      m_opacity_slider->set_value(100.0f);
      m_opacity_textbox->set_value("100");
      m_rotation_textbox->set_value("0");
      m_position_x_textbox->set_value("0");
      m_position_y_textbox->set_value("0");
      m_size_width_textbox->set_value("0");
      m_size_height_textbox->set_value("0");
      m_fill_none_checkbox->set_checked(false);
    } else if (selected_indices.size() == 1) {
      // Single selection - show exact values
      int idx = selected_indices[0];
      if (idx >= 0 && idx < (int)strokes.size()) {
        const Stroke &stroke = strokes[idx];
        set_title("Properties (1 selected)");

        m_stroke_color_picker->set_color(stroke.color);
        m_fill_color_picker->set_color(stroke.fill_color);
        m_stroke_width_slider->set_value(stroke.width);
        m_width_textbox->set_value(std::to_string((int)stroke.width));

        // Opacity (assuming it's stored in alpha channel)
        float opacity = stroke.color.w();
        m_opacity_slider->set_value(opacity * 100.0f);
        m_opacity_textbox->set_value(std::to_string((int)(opacity * 100.0f)));

        // Rotation in degrees
        float degrees = stroke.rotation * 180.0f / M_PI;
        while (degrees < 0)
          degrees += 360.0f;
        while (degrees >= 360.0f)
          degrees -= 360.0f;
        m_rotation_textbox->set_value(std::to_string((int)degrees));

        // Position (first point)
        if (!stroke.points.empty()) {
          m_position_x_textbox->set_value(std::to_string((int)stroke.points[0].x));
          m_position_y_textbox->set_value(std::to_string((int)stroke.points[0].y));
        }

        // Size (for rectangles)
        if (stroke.tool == Tool::Rectangle && stroke.points.size() >= 2) {
          float width = std::abs(stroke.points[1].x - stroke.points[0].x);
          float height = std::abs(stroke.points[1].y - stroke.points[0].y);
          m_size_width_textbox->set_value(std::to_string((int)width));
          m_size_height_textbox->set_value(std::to_string((int)height));
        }

        m_fill_none_checkbox->set_checked(stroke.fill_style == FillStyle::None);
      }
    } else {
      // Multiple selection - show common values or "Mixed"
      set_title("Properties (" + std::to_string(selected_indices.size()) + " selected)");

      // Check if all have same stroke color
      bool same_stroke_color = true;
      Color first_stroke_color = strokes[selected_indices[0]].color;
      for (int idx : selected_indices) {
        if (idx >= 0 && idx < (int)strokes.size()) {
          if (strokes[idx].color != first_stroke_color) {
            same_stroke_color = false;
            break;
          }
        }
      }
      if (same_stroke_color) {
        m_stroke_color_picker->set_color(first_stroke_color);
      }

      // Check if all have same stroke width
      bool same_width = true;
      float first_width = strokes[selected_indices[0]].width;
      for (int idx : selected_indices) {
        if (idx >= 0 && idx < (int)strokes.size()) {
          if (std::abs(strokes[idx].width - first_width) > 0.01f) {
            same_width = false;
            break;
          }
        }
      }
      if (same_width) {
        m_stroke_width_slider->set_value(first_width);
        m_width_textbox->set_value(std::to_string((int)first_width));
      } else {
        m_width_textbox->set_value("Mixed");
      }

      // For other properties, show "Mixed" or first value
      m_rotation_textbox->set_value("Mixed");
      m_position_x_textbox->set_value("Mixed");
      m_position_y_textbox->set_value("Mixed");
      m_size_width_textbox->set_value("Mixed");
      m_size_height_textbox->set_value("Mixed");
    }

    m_selected_indices = selected_indices;
    m_updating = false;
  }

private:
  ModernCanvas *m_canvas;
  ColorPicker *m_stroke_color_picker;
  ColorPicker *m_fill_color_picker;
  CheckBox *m_fill_none_checkbox;
  Slider *m_stroke_width_slider;
  Slider *m_opacity_slider;
  TextBox *m_rotation_textbox;
  TextBox *m_position_x_textbox;
  TextBox *m_position_y_textbox;
  TextBox *m_size_width_textbox;
  TextBox *m_size_height_textbox;
  TextBox *m_width_textbox;
  TextBox *m_opacity_textbox;
  std::vector<int> m_selected_indices;
  bool m_updating = false;

  void on_stroke_color_changed(const Color &color);
  void on_fill_color_changed(const Color &color);
  void on_fill_none_changed(bool none);
  void on_stroke_width_changed(float width);
  void on_opacity_changed(float opacity);
  void on_rotation_changed(float radians);
  void on_position_changed(float x, float y);
  void on_size_changed(float width, float height);
};

} // namespace whiteboard