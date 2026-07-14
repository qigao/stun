/*
 * flexUI - DropdownWidget
 *
 * Select from a list of options - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_DROPDOWN_WIDGET_H
#define FLEXUI_DROPDOWN_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <vector>
#include <functional>

namespace flexUI {

class RenderCommandList;

/**
 * DropdownWidget - Selection dropdown
 *
 * CSS variables:
 *   --dropdown-bg: "r,g,b,a"           // Background color
 *   --dropdown-text: "r,g,b,a"         // Text color
 *   --dropdown-border: "r,g,b,a"       // Border color
 *   --dropdown-item-hover: "r,g,b,a"   // Item hover background
 *   --dropdown-max-height: "200"       // Max dropdown height
 */
class DropdownWidget : public Widget {
public:
  struct Option {
    std::string label;
    std::string value;
    bool disabled = false;
  };

  explicit DropdownWidget(const std::string& placeholder = "Select...");

  void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
  void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override;
  bool has_overlay() const override { return open_; }
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "DropdownWidget"; }
  bool paints_host_box() const override { return true; }
  bool wants_mouse_capture() const override { return open_; }

  // Options
  void add_option(const std::string& label, const std::string& value, bool disabled = false);
  void clear_options();
  const std::vector<Option>& options() const { return options_; }

  // Selection
  const std::string& selected_value() const { return selected_value_; }
  void set_selected_value(const std::string& value);
  int selected_index() const { return selected_index_; }
  void set_selected_index(int index);

  // Placeholder
  const std::string& placeholder() const { return placeholder_; }
  void set_placeholder(const std::string& p) { placeholder_ = p; dirty_ = true; }

  // State
  bool is_open() const { return open_; }
  void open();
  void close();
  void toggle();

  // Callback
  using ChangeCallback = std::function<void(const std::string& value)>;
  void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
  void sync_host_semantics() override;
  void render_button(RenderCommandList& commands, const Element& elem);
  void render_arrow(RenderCommandList& commands, const Element& elem);
  void render_dropdown(RenderCommandList& commands, const Element& elem);

  std::string placeholder_;
  std::vector<Option> options_;
  std::string selected_value_;
  int selected_index_ = -1;
  int hover_index_ = -1;
  bool open_ = false;
  float scroll_offset_ = 0;
  ChangeCallback on_change_;
};

} // namespace flexUI

#endif // FLEXUI_DROPDOWN_WIDGET_H
