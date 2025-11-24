#pragma once

#include "whiteboard/common.h"
#include "whiteboard/model/whiteboard_document.h"

namespace whiteboard {

class SearchBar : public Window {
public:
  SearchBar(Widget *parent, WhiteboardDocument *document)
      : Window(parent, ""), m_document(document), m_current_result_index(-1) {
    set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 10));
    set_fixed_size(Vector2i(400, 50));

    // Search icon
    auto *icon_label = new Label(this, std::string(utf8(FA_SEARCH).data()), "icons");
    icon_label->set_font_size(18);
    icon_label->set_color(Color(100, 100, 100, 255));

    // Search input field
    m_search_input = new TextBox(this, "");
    m_search_input->set_placeholder("Search shapes...");
    m_search_input->set_fixed_size(Vector2i(200, 25));
    m_search_input->set_editable(true);
    m_search_input->set_callback([this](const std::string &value) {
      on_search_query_changed(value);
      return true;
    });

    // Result count label
    m_result_label = new Label(this, "0 of 0", "sans", 14);
    m_result_label->set_color(Color(100, 100, 100, 255));

    // Previous button
    auto *prev_btn = new Button(this, "", FA_CHEVRON_UP);
    prev_btn->set_fixed_size(Vector2i(30, 30));
    prev_btn->set_callback([this]() { previous_result(); });

    // Next button
    auto *next_btn = new Button(this, "", FA_CHEVRON_DOWN);
    next_btn->set_fixed_size(Vector2i(30, 30));
    next_btn->set_callback([this]() { next_result(); });

    // Close button
    auto *close_btn = new Button(this, "", FA_TIMES);
    close_btn->set_fixed_size(Vector2i(30, 30));
    close_btn->set_callback([this]() {
      clear_search();
      set_visible(false);
    });
  }

  void on_search_query_changed(const std::string &query) {
    if (!m_document)
      return;

    m_search_results.clear();
    m_current_result_index = -1;

    if (query.empty()) {
      m_result_label->set_caption("0 of 0");
      // Clear highlights - would need to be implemented in document
      return;
    }

    // Convert query to lowercase for case-insensitive search
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    // Search through all strokes
    const auto &strokes = m_document->get_strokes();
    for (int i = 0; i < (int)strokes.size(); i++) {
      const auto &stroke = strokes[i];

      // Skip invisible strokes
      if (!stroke.visible)
        continue;

      bool matches = false;

      // Match by object type
      std::string type_name = get_tool_name(stroke.tool);
      std::transform(type_name.begin(), type_name.end(), type_name.begin(), ::tolower);
      if (type_name.find(lower_query) != std::string::npos) {
        matches = true;
      }

      // Match by text content (for text strokes)
      if (!matches && stroke.tool == Tool::Text && !stroke.text.empty()) {
        std::string lower_text = stroke.text;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        if (lower_text.find(lower_query) != std::string::npos) {
          matches = true;
        }
      }

      // Match by color (hex codes and color names)
      if (!matches) {
        std::string color_str = color_to_string(stroke.color);
        std::transform(color_str.begin(), color_str.end(), color_str.begin(), ::tolower);
        if (color_str.find(lower_query) != std::string::npos) {
          matches = true;
        }
      }

      if (matches) {
        m_search_results.push_back(i);
      }
    }

    // Update result label
    if (m_search_results.empty()) {
      m_result_label->set_caption("0 of 0");
    } else {
      m_current_result_index = 0;
      m_result_label->set_caption(std::to_string(m_current_result_index + 1) + " of " +
                                  std::to_string(m_search_results.size()));
      // Set selection to highlight the result
      if (!m_search_results.empty()) {
        std::vector<int> selection = {m_search_results[m_current_result_index]};
        m_document->set_selection(selection);
      }
      pan_to_current_result();
    }
  }

  void next_result() {
    if (m_search_results.empty())
      return;

    m_current_result_index = (m_current_result_index + 1) % m_search_results.size();
    m_result_label->set_caption(std::to_string(m_current_result_index + 1) + " of " +
                                std::to_string(m_search_results.size()));
    if (m_document) {
      std::vector<int> selection = {m_search_results[m_current_result_index]};
      m_document->set_selection(selection);
    }
    pan_to_current_result();
  }

  void previous_result() {
    if (m_search_results.empty())
      return;

    m_current_result_index--;
    if (m_current_result_index < 0) {
      m_current_result_index = m_search_results.size() - 1;
    }
    m_result_label->set_caption(std::to_string(m_current_result_index + 1) + " of " +
                                std::to_string(m_search_results.size()));
    if (m_document) {
      std::vector<int> selection = {m_search_results[m_current_result_index]};
      m_document->set_selection(selection);
    }
    pan_to_current_result();
  }

  void clear_search() {
    m_search_input->set_value("");
    m_search_results.clear();
    m_current_result_index = -1;
    m_result_label->set_caption("0 of 0");
    if (m_document) {
      m_document->clear_selection();
    }
  }

  void focus_search_input() {
    if (m_search_input) {
      m_search_input->request_focus();
    }
  }

  bool handle_keyboard_event(int key, int modifiers) {
    if (!visible())
      return false;

    // Handle Enter to cycle forward
    if (key == NANOGUI_KEY_ENTER) {
      if (modifiers & NANOGUI_MOD_SHIFT) {
        previous_result();
      } else {
        next_result();
      }
      return true;
    }

    // Handle Escape to close
    if (key == NANOGUI_KEY_ESCAPE) {
      clear_search();
      set_visible(false);
      return true;
    }

    return false;
  }

private:
  WhiteboardDocument *m_document;
  TextBox *m_search_input;
  Label *m_result_label;
  std::vector<int> m_search_results;
  int m_current_result_index;

  std::string get_tool_name(Tool tool) const {
    switch (tool) {
    case Tool::Select:
      return "select";
    case Tool::Pan:
      return "pan";
    case Tool::Pen:
      return "pen";
    case Tool::Text:
      return "text";
    case Tool::Sticky:
      return "sticky";
    case Tool::Rectangle:
      return "rectangle";
    case Tool::Circle:
      return "circle";
    case Tool::Line:
      return "line";
    case Tool::Arrow:
      return "arrow";
    default:
      return "unknown";
    }
  }

  std::string color_to_string(const Color &color) const {
    // Convert to hex string
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)(color.r() * 255), (int)(color.g() * 255),
             (int)(color.b() * 255));
    std::string result = hex;

    // Add common color names
    if (color.r() > 0.8f && color.g() < 0.3f && color.b() < 0.3f)
      result += " red";
    else if (color.r() < 0.3f && color.g() > 0.8f && color.b() < 0.3f)
      result += " green";
    else if (color.r() < 0.3f && color.g() < 0.3f && color.b() > 0.8f)
      result += " blue";
    else if (color.r() > 0.8f && color.g() > 0.8f && color.b() < 0.3f)
      result += " yellow";
    else if (color.r() > 0.8f && color.g() < 0.3f && color.b() > 0.8f)
      result += " magenta";
    else if (color.r() < 0.3f && color.g() > 0.8f && color.b() > 0.8f)
      result += " cyan";
    else if (color.r() > 0.8f && color.g() > 0.5f && color.b() < 0.3f)
      result += " orange";
    else if (color.r() > 0.5f && color.g() < 0.3f && color.b() > 0.5f)
      result += " purple";
    else if (color.r() > 0.8f && color.g() > 0.7f && color.b() > 0.7f)
      result += " pink";
    else if (color.r() < 0.2f && color.g() < 0.2f && color.b() < 0.2f)
      result += " black";
    else if (color.r() > 0.8f && color.g() > 0.8f && color.b() > 0.8f)
      result += " white";
    else if (color.r() > 0.4f && color.g() > 0.4f && color.b() > 0.4f && color.r() < 0.6f &&
             color.g() < 0.6f && color.b() < 0.6f)
      result += " gray";

    return result;
  }

  void pan_to_current_result() {
    if (!m_document || m_current_result_index < 0 ||
        m_current_result_index >= (int)m_search_results.size()) {
      return;
    }

    // Pan to stroke - would need to calculate bounds and update pan offset
    // For now, this is a placeholder
    const auto &strokes = m_document->get_strokes();
    int stroke_idx = m_search_results[m_current_result_index];
    if (stroke_idx >= 0 && stroke_idx < static_cast<int>(strokes.size())) {
      const auto &stroke = strokes[stroke_idx];
      if (!stroke.points.empty()) {
        // Center on first point of the stroke
        Vector2f center(stroke.points[0].x, stroke.points[0].y);
        // This would need canvas dimensions to properly center
        // m_document->set_pan_offset(center);
      }
    }
  }
};


}
