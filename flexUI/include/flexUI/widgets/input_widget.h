/*
 * flexUI - InputWidget
 *
 * 文本输入控件 - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_INPUT_WIDGET_H
#define FLEXUI_INPUT_WIDGET_H

#include "../textedit.h"
#include "../text_value_widget.h"
#include "../widget.h"
#include "../render_command.h"
#include "../group.h"
#include "../shapes.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace flexUI {

/**
 * InputWidget - 文本输入框
 *
 * CSS 变量：
 *   --input-bg: 背景色
 *   --input-text: 文字颜色
 *   --input-border: 边框颜色
 *   --input-placeholder: placeholder 颜色
 *   --input-selection-bg: 选中背景色
 *   --input-cursor: 光标颜色
 */
class InputWidget : public Widget, public TextValueWidget {
public:
  enum class Variant {
    Default,
    Filled,
    Ghost,
  };

  enum class Size {
    Small,
    Default,
    Large,
  };

  explicit InputWidget(const std::string &placeholder = "", bool password = false);

  void emit_render_commands(const Element &elem, RenderCommandList &commands) override;
  bool handle_event(const Event &event, Element &elem) override;
  void update(float delta_ms, Element &elem) override;
  bool needs_frame_update(const Element& elem) const override;
  bool measure_intrinsic_size(const Element& elem, float available_width,
                              float available_height, float& out_width,
                              float& out_height) const override;
  const char *type_name() const override { return "InputWidget"; }
  bool wants_mouse_capture() const override { return is_dragging_; }
  bool wants_text_input() const override { return !readonly_ && !disabled_; }
  bool paints_host_box() const override { return true; }
  bool paints_part_box(std::string_view part_name) const override;
  bool emit_part_render_commands(const Element& host, const Element& part,
                                 std::string_view part_name,
                                 RenderCommandList& commands) override;
  void sync_host_semantics_for_layout(Element& elem) override;
  void get_caret_rect(const Element& elem, float& x, float& y, float& w, float& h) const override;

  Element* viewport_element() { return viewport_; }
  Element* selection_layer_element() { return selection_layer_; }
  Element* text_element() { return text_element_; }
  Element* placeholder_element() { return placeholder_element_; }
  Element* caret_element() { return caret_element_; }
  const Element* viewport_element() const { return viewport_; }
  const Element* selection_layer_element() const { return selection_layer_; }
  const Element* text_element() const { return text_element_; }
  const Element* placeholder_element() const { return placeholder_element_; }
  const Element* caret_element() const { return caret_element_; }

  // Text access
  const std::string &text() const { return text_; }
  void set_text(const std::string &text);
  const std::string& text_value() const override { return text_; }
  void set_text_value(const std::string& value) override { set_text(value); }
  TextValueObserverId add_edit_observer(EditObserver observer) override;
  bool remove_edit_observer(TextValueObserverId id) override;

  const std::string &placeholder() const { return placeholder_; }
  void set_placeholder(const std::string &text) {
    placeholder_ = text;
    invalidate_render_cache();
    sync_host_semantics();
  }

  bool is_password() const { return password_; }
  void set_password(bool enabled) { password_ = enabled; invalidate_render_cache(); }

  bool is_readonly() const { return readonly_; }
  void set_readonly(bool readonly) {
    readonly_ = readonly;
    invalidate_render_cache();
    sync_host_semantics();
  }

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) {
    disabled_ = disabled;
    invalidate_render_cache();
    sync_host_semantics();
  }

  Variant variant() const { return variant_; }
  void set_variant(Variant variant) { variant_ = variant; invalidate_render_cache(); }

  Size size() const { return size_; }
  void set_size(Size size) { size_ = size; invalidate_render_cache(); }

  // Selection and cursor
  size_t cursor_pos() const;
  void set_cursor_pos(size_t pos);

  bool has_selection() const { return textedit_ && textedit_->has_selection(); }
  std::string selected_text() const;
  void select_all();
  void clear_selection();

  // Callback
  using ChangeCallback = std::function<void(const std::string &text)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  void notify_edit_observers();
  void build_semantic_tree() override;
  void sync_host_semantics() override;
  void update_part_geometry(const Element& elem);
  void render_background(RenderCommandList& commands, const Element &elem);
  void render_text(RenderCommandList& commands, const Element &elem,
                   const ComputedStyle* part_style = nullptr);
  void render_cursor(RenderCommandList& commands, const Element &elem,
                     const ComputedStyle* part_style = nullptr);
  void render_selection(RenderCommandList& commands, const Element &elem,
                        const ComputedStyle* part_style = nullptr);
  void render_composition(RenderCommandList& commands, const Element& elem);
  bool render_cache_matches(const Element& elem) const;
  void update_render_cache_key(const Element& elem);
  void invalidate_render_cache();
  bool can_use_render_cache(const Element& elem) const;

  bool handle_key_down(const Event &event, Element &elem);
  bool handle_text_input(const Event &event, Element &elem);
  bool handle_composition_start(const Event& event, Element& elem);
  bool handle_composition_update(const Event& event, Element& elem);
  bool handle_composition_end(const Event& event, Element& elem);
  bool handle_mouse_down(const Event &event, Element &elem);
  bool handle_mouse_move(const Event &event, Element &elem);
  bool handle_mouse_up(const Event &event, Element &elem);

  size_t x_to_index(float x, const Element &elem) const;
  float index_to_x(size_t index, const Element &elem) const;
  std::string display_text() const;
  void ensure_textedit_init(const Element &elem);

  std::string text_;
  std::string placeholder_;
  bool password_ = false;
  bool readonly_ = false;
  bool disabled_ = false;

  std::unique_ptr<TextEdit> textedit_;
  bool textedit_initialized_ = false;

  ChangeCallback change_callback_;
  std::vector<std::pair<TextValueObserverId, EditObserver>> edit_observers_;
  TextValueObserverId next_edit_observer_id_ = 1;
  bool is_dragging_ = false;
  int drag_anchor_ = 0;

  float cursor_blink_time_ = 0;
  bool cursor_visible_ = true;
  bool is_composing_ = false;
  std::string composition_text_;
  Variant variant_ = Variant::Default;
  Size size_ = Size::Default;

  Element* viewport_ = nullptr;
  Element* selection_layer_ = nullptr;
  Element* text_element_ = nullptr;
  Element* placeholder_element_ = nullptr;
  Element* caret_element_ = nullptr;

  bool render_cache_valid_ = false;
  bool cached_hover_state_ = false;
  bool cached_disabled_state_ = false;
  bool cached_readonly_state_ = false;
  bool cached_password_ = false;
  int cached_variant_ = 0;
  int cached_size_ = 0;
  uint64_t cached_style_signature_ = 0;
  float cached_width_ = 0.0f;
  float cached_height_ = 0.0f;
  float cached_opacity_ = 1.0f;
  float cached_font_size_ = 0.0f;
  float cached_letter_spacing_ = 0.0f;
  float cached_word_spacing_ = 0.0f;
  float cached_tab_size_ = 8.0f;
  float cached_padding_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float cached_border_radius_ = 0.0f;
  int cached_font_weight_ = 0;
  int cached_font_style_ = 0;
  int cached_direction_ = 0;
  std::string cached_text_;
  std::string cached_placeholder_;
  std::string cached_font_family_;
  Color cached_background_color_;
  Color cached_text_color_;
  Color cached_border_color_;
  std::vector<RenderCommand> render_cache_;
};

} // namespace flexUI

#endif // FLEXUI_INPUT_WIDGET_H
