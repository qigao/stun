/*
 * flexUI - TextAreaWidget
 *
 * 多行文本输入 - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_TEXTAREA_WIDGET_H
#define FLEXUI_TEXTAREA_WIDGET_H

#include "../widget.h"
#include "../text_value_widget.h"
#include "../render_command.h"
#include "../group.h"
#include "../shapes.h"
#include <cstdint>
#include <string>
#include <functional>
#include <utility>
#include <vector>

namespace flexUI {

/**
 * TextAreaWidget - 多行文本输入
 *
 * CSS 变量支持：
 *   --textarea-bg: "r,g,b,a"
 *   --textarea-text: "r,g,b,a"
 *   --textarea-border: "r,g,b,a"
 *   --textarea-placeholder: "r,g,b,a"
 *   --textarea-cursor: "r,g,b,a"
 *   --textarea-selection-bg: "r,g,b,a"
 */
class TextAreaWidget : public Widget, public TextValueWidget {
public:
  explicit TextAreaWidget(const std::string& text = "",
                          const std::string& placeholder = "");

  void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  bool needs_frame_update(const Element& elem) const override;
  bool wants_mouse_capture() const override { return is_dragging_; }
  const char* type_name() const override { return "TextAreaWidget"; }
  bool wants_text_input() const override { return !readonly_ && !disabled_; }
  bool paints_host_box() const override { return true; }
  bool paints_part_box(std::string_view part_name) const override;
  bool emit_part_render_commands(const Element& host, const Element& part,
                                 std::string_view part_name,
                                 RenderCommandList& commands) override;
  void sync_host_semantics_for_layout(Element& elem) override;
  void get_caret_rect(const Element& elem, float& x, float& y, float& w, float& h) const override;
  bool measure_intrinsic_size(const Element& elem, float available_width,
                              float available_height, float& out_width,
                              float& out_height) const override;

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

  const std::string& text() const { return text_; }
  void set_text(const std::string& text);
  const std::string& text_value() const override { return text_; }
  void set_text_value(const std::string& value) override { set_text(value); }
  TextValueObserverId add_edit_observer(EditObserver observer) override;
  bool remove_edit_observer(TextValueObserverId id) override;

  const std::string& placeholder() const { return placeholder_; }
  void set_placeholder(const std::string& placeholder) {
    placeholder_ = placeholder;
    invalidate_render_cache();
    sync_host_semantics();
  }

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

  int cursor_position() const { return cursor_pos_; }
  void set_cursor_position(int pos);

  int selection_start() const;
  int selection_end() const;
  bool has_selection() const;
  void set_selection(int start, int end);
  void clear_selection();
  std::string selected_text() const;

  int line_count() const { return static_cast<int>(lines_.size()); }
  int position_from_line_column(int line, int column) const;
  std::pair<int, int> line_column_from_position(int position) const;

  float scroll_offset() const { return scroll_offset_; }
  void set_scroll_offset(float offset);

  using ChangeCallback = std::function<void(const std::string& text)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  void notify_edit_observers();
  void build_semantic_tree() override;
  void sync_host_semantics() override;
  void update_part_geometry(const Element& elem);
  void split_lines();
  void rebuild_text();

  int get_line_from_cursor() const;
  int get_column_from_cursor() const;
  void move_cursor_to_line_column(int line, int col);

  void render_background(RenderCommandList& commands, const Element& elem);
  void render_text_lines(RenderCommandList& commands, const Element& elem,
                         const ComputedStyle* part_style = nullptr);
  void render_placeholder(RenderCommandList& commands, const Element& elem,
                          const ComputedStyle* part_style = nullptr);
  void render_selection(RenderCommandList& commands, const Element& elem,
                        const ComputedStyle* part_style = nullptr);
  void render_cursor(RenderCommandList& commands, const Element& elem,
                     const ComputedStyle* part_style = nullptr);
  void render_composition(RenderCommandList& commands, const Element& elem);
  bool can_use_render_cache(const Element& elem) const;
  bool render_cache_matches(const Element& elem) const;
  void update_render_cache_key(const Element& elem);
  void invalidate_render_cache();

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_mouse_move(const Event& event, Element& elem);
  bool handle_mouse_up(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);
  bool handle_text_input(const Event& event, Element& elem);
  bool handle_composition_start(const Event& event, Element& elem);
  bool handle_composition_update(const Event& event, Element& elem);
  bool handle_composition_end(const Event& event, Element& elem);

  bool copy_selection_to_clipboard() const;
  bool cut_selection_to_clipboard();
  bool paste_from_clipboard();

  void insert_text(const std::string& str);
  void delete_selection();
  void delete_char_before_cursor();
  void delete_char_after_cursor();
  bool undo_edit();
  bool redo_edit();

  void update_cursor_blink(float delta_ms);

  struct EditSnapshot {
    std::string text;
    int cursor_pos = 0;
    int selection_anchor = -1;
    int selection_start = -1;
    int selection_end = -1;
  };

  EditSnapshot capture_snapshot() const;
  void restore_snapshot(const EditSnapshot& snapshot);
  void push_undo_snapshot();

  std::string text_;
  std::string placeholder_;
  std::vector<std::string> lines_;

  int cursor_pos_ = 0;
  int selection_anchor_ = -1;
  int selection_start_ = -1;
  int selection_end_ = -1;

  bool readonly_ = false;
  bool disabled_ = false;

  float cursor_blink_time_ = 0.0f;
  bool cursor_visible_ = true;

  float scroll_offset_ = 0.0f;

  ChangeCallback change_callback_;
  std::vector<std::pair<TextValueObserverId, EditObserver>> edit_observers_;
  TextValueObserverId next_edit_observer_id_ = 1;
  bool is_dragging_ = false;

  bool is_composing_ = false;
  std::string composition_text_;

  Element* viewport_ = nullptr;
  Element* selection_layer_ = nullptr;
  Element* text_element_ = nullptr;
  Element* placeholder_element_ = nullptr;
  Element* caret_element_ = nullptr;

  std::vector<EditSnapshot> undo_stack_;
  std::vector<EditSnapshot> redo_stack_;

  static constexpr size_t kMaxHistoryEntries = 64;

  bool render_cache_valid_ = false;
  bool cached_disabled_state_ = false;
  bool cached_readonly_state_ = false;
  uint64_t cached_style_signature_ = 0;
  float cached_width_ = 0.0f;
  float cached_height_ = 0.0f;
  float cached_opacity_ = 1.0f;
  float cached_font_size_ = 0.0f;
  float cached_letter_spacing_ = 0.0f;
  float cached_word_spacing_ = 0.0f;
  float cached_tab_size_ = 8.0f;
  float cached_scroll_offset_ = 0.0f;
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

#endif // FLEXUI_TEXTAREA_WIDGET_H
