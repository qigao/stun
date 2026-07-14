/*
 * flexUI - TextAreaWidget Implementation
 */

#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_input_common.h>
#include <flexUI/text_layout.h>
#include <flexUI/text_util.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace flexUI {

namespace {

bool approx_equal(float lhs, float rhs) {
  return std::fabs(lhs - rhs) <= 0.001f;
}

bool is_default_background(const Color& color) {
  return approx_equal(color.r, 1.0f) && approx_equal(color.g, 1.0f) &&
         approx_equal(color.b, 1.0f) && approx_equal(color.a, 0.0f);
}

bool is_default_border(const Color& color) {
  return approx_equal(color.r, 0.0f) && approx_equal(color.g, 0.0f) &&
         approx_equal(color.b, 0.0f) && approx_equal(color.a, 1.0f);
}

bool is_rtl(const ComputedStyle* style) {
  return style && style->direction == Direction::Rtl;
}

float effective_letter_spacing(const ComputedStyle* style) {
  return style ? std::max(style->letter_spacing, 0.0f) : 0.0f;
}

float effective_word_spacing(const ComputedStyle* style) {
  return style ? std::max(style->word_spacing, 0.0f) : 0.0f;
}

bool is_word_spacing_gap(uint32_t cp) {
  return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\f';
}

size_t count_word_spacing_gaps(const std::string& text) {
  size_t count = 0;
  size_t pos = 0;
  while (pos < text.size()) {
    const uint32_t cp = utf8_decode(text, pos);
    if (is_word_spacing_gap(cp)) {
      ++count;
    }
  }
  return count;
}

size_t utf8_codepoint_count(const std::string& text) {
  size_t count = 0;
  size_t pos = 0;
  while (pos < text.size()) {
    utf8_decode(text, pos);
    ++count;
  }
  return count;
}

int next_utf8_offset(const std::string& text, int byte_offset) {
  size_t pos = static_cast<size_t>(
      std::max(0, std::min(byte_offset, static_cast<int>(text.size()))));
  if (pos >= text.size()) {
    return static_cast<int>(text.size());
  }
  utf8_decode(text, pos);
  return static_cast<int>(pos);
}

int previous_utf8_offset(const std::string& text, int byte_offset) {
  const size_t target = static_cast<size_t>(
      std::max(0, std::min(byte_offset, static_cast<int>(text.size()))));
  if (target == 0) {
    return 0;
  }

  size_t previous = 0;
  size_t pos = 0;
  while (pos < target) {
    previous = pos;
    utf8_decode(text, pos);
  }
  return static_cast<int>(previous);
}

int clamp_utf8_offset(const std::string& text, int byte_offset) {
  const int clamped =
      std::max(0, std::min(byte_offset, static_cast<int>(text.size())));
  if (clamped == 0 || clamped == static_cast<int>(text.size())) {
    return clamped;
  }

  size_t pos = 0;
  while (pos < static_cast<size_t>(clamped)) {
    const size_t start = pos;
    utf8_decode(text, pos);
    if (pos > static_cast<size_t>(clamped)) {
      return static_cast<int>(start);
    }
  }
  return clamped;
}

ComputedStyle make_textarea_measure_style(const ComputedStyle* style,
                                          const EditableTextMetrics& metrics) {
  ComputedStyle measure_style;
  if (style) {
    measure_style = *style;
  }
  measure_style.font_family = metrics.font_family;
  measure_style.font_size = metrics.font_size;
  measure_style.text_transform = TextTransform::None;
  measure_style.letter_spacing = 0.0f;
  measure_style.word_spacing = 0.0f;
  return measure_style;
}

float textarea_content_right(const Element& elem,
                             const EditableTextMetrics& metrics) {
  return elem.width() - metrics.padding_right;
}

float textarea_line_width(const std::string& line, const ComputedStyle* style,
                          const EditableTextMetrics& metrics) {
  const ComputedStyle measure_style = make_textarea_measure_style(style, metrics);
  const std::string display = transform_text_for_layout(style, line);
  float width = 0.0f;
  size_t codepoint_count = 0;
  size_t word_gap_count = 0;
  for (const auto& segment : segment_text(display)) {
    if (segment.type == TextSegmentType::Emoji) {
      const size_t count = utf8_codepoint_count(segment.text);
      width += static_cast<float>(count) * metrics.font_size;
      codepoint_count += count;
      continue;
    }
    width += approximate_text_width(&measure_style, segment.text);
    const size_t count = utf8_codepoint_count(segment.text);
    codepoint_count += count;
    word_gap_count += count_word_spacing_gaps(segment.text);
  }
  if (codepoint_count > 1) {
    width += effective_letter_spacing(style) *
             static_cast<float>(codepoint_count - 1);
  }
  width += effective_word_spacing(style) * static_cast<float>(word_gap_count);
  return width;
}

float textarea_prefix_width(const std::string& line, const ComputedStyle* style,
                            const EditableTextMetrics& metrics, int byte_count) {
  const int clamped = std::max(0, std::min(byte_count, static_cast<int>(line.size())));
  return textarea_line_width(line.substr(0, static_cast<size_t>(clamped)), style, metrics);
}

float textarea_line_draw_x(const Element& elem, const ComputedStyle* style,
                           const EditableTextMetrics& metrics,
                           const std::string& line) {
  if (!is_rtl(elem.computed_style)) {
    return metrics.padding_left;
  }
  return textarea_content_right(elem, metrics) -
         textarea_line_width(line, style, metrics);
}

float textarea_caret_x(const Element& elem, const ComputedStyle* style,
                       const EditableTextMetrics& metrics,
                       const std::string& line, int col) {
  const float prefix_width = textarea_prefix_width(line, style, metrics, col);
  if (!is_rtl(elem.computed_style)) {
    return metrics.padding_left + prefix_width;
  }
  return textarea_content_right(elem, metrics) - prefix_width;
}

int textarea_column_from_x(const Element& elem, const ComputedStyle* style,
                           const EditableTextMetrics& metrics,
                           const std::string& line, float local_x, int max_col) {
  float relative_x = 0.0f;
  if (!is_rtl(elem.computed_style)) {
    relative_x = local_x - metrics.padding_left;
  } else {
    relative_x = textarea_content_right(elem, metrics) - local_x;
  }
  if (relative_x <= 0.0f) {
    return 0;
  }

  float advance = 0.0f;
  size_t byte_pos = 0;
  while (byte_pos < line.size()) {
    const size_t start = byte_pos;
    utf8_decode(line, byte_pos);
    const float glyph_width = textarea_line_width(
        line.substr(start, byte_pos - start), style, metrics);
    const float glyph_advance =
        glyph_width + (byte_pos < line.size() ? effective_letter_spacing(style) : 0.0f);
    if (relative_x < advance + glyph_advance * 0.5f) {
      return std::max(0, std::min(static_cast<int>(start), max_col));
    }
    advance += glyph_advance;
  }
  return std::max(0, std::min(static_cast<int>(line.size()), max_col));
}

float textarea_segment_width(const TextSegment& segment,
                             const ComputedStyle* style,
                             const EditableTextMetrics& metrics) {
  if (segment.type == TextSegmentType::Emoji) {
    size_t count = 0;
    size_t pos = 0;
    while (pos < segment.text.size()) {
      utf8_decode(segment.text, pos);
      count++;
    }
    return static_cast<float>(count) * metrics.font_size;
  }
  return textarea_line_width(segment.text, style, metrics);
}

float draw_textarea_segmented(RenderCommandList& commands, const std::string& text,
                              float x, float y, const ComputedStyle* style,
                              const EditableTextMetrics& metrics,
                              const Color& color) {
  float current_x = x;
  const float letter_spacing = effective_letter_spacing(style);
  const float word_spacing = effective_word_spacing(style);
  const ComputedStyle measure_style = make_textarea_measure_style(style, metrics);
  const std::string display = transform_text_for_layout(style, text);
  const auto segments = segment_text(display);
  const bool bold = style && style->font_weight >= FontWeight::Bold;
  for (size_t i = 0; i < segments.size(); ++i) {
    const auto& segment = segments[i];
    const std::string font_name =
        segment.type == TextSegmentType::Emoji ? get_emoji_font_name()
                                               : metrics.font_family;
    const bool has_tab = segment.text.find('\t') != std::string::npos;
    if (segment.type == TextSegmentType::Emoji ||
        (letter_spacing <= 0.0f && word_spacing <= 0.0f && !has_tab)) {
      commands.draw_text(segment.text, current_x, y, font_name, metrics.font_size,
                         bold, color);
      current_x += textarea_segment_width(segment, style, metrics);
    } else if (letter_spacing <= 0.0f) {
      size_t byte_pos = 0;
      while (byte_pos < segment.text.size()) {
        const size_t run_start = byte_pos;
        const uint32_t first_cp = utf8_decode(segment.text, byte_pos);
        const bool gap_run = is_word_spacing_gap(first_cp);
        while (byte_pos < segment.text.size()) {
          const size_t before = byte_pos;
          const uint32_t cp = utf8_decode(segment.text, byte_pos);
          if (is_word_spacing_gap(cp) != gap_run) {
            byte_pos = before;
            break;
          }
        }
        const std::string run =
            segment.text.substr(run_start, byte_pos - run_start);
        const float run_width = approximate_text_width(&measure_style, run);
        if (!gap_run) {
          commands.draw_text(run, current_x, y, font_name, metrics.font_size,
                             bold, color);
        }
        current_x += run_width > 0.0f
                         ? run_width
                         : metrics.font_size * 0.5f *
                               static_cast<float>(utf8_codepoint_count(run));
        current_x += word_spacing *
                     static_cast<float>(count_word_spacing_gaps(run));
      }
    } else {
      size_t byte_pos = 0;
      while (byte_pos < segment.text.size()) {
        const size_t start = byte_pos;
        const uint32_t cp = utf8_decode(segment.text, byte_pos);
        const std::string glyph = segment.text.substr(start, byte_pos - start);
        const float glyph_width = approximate_text_width(&measure_style, glyph);
        if (!is_word_spacing_gap(cp)) {
          commands.draw_text(glyph, current_x, y, font_name, metrics.font_size,
                             bold, color);
        }
        current_x += glyph_width > 0.0f ? glyph_width : metrics.font_size * 0.5f;
        if (is_word_spacing_gap(cp)) {
          current_x += word_spacing;
        }
        if (byte_pos < segment.text.size()) {
          current_x += letter_spacing;
        }
      }
    }
    if (i + 1 < segments.size()) {
      current_x += letter_spacing;
    }
  }
  return current_x - x;
}

float draw_textarea_segmented_with_shadows(
    RenderCommandList& commands, const std::string& text, float x, float y,
    const ComputedStyle* style, const EditableTextMetrics& metrics,
    const Color& color) {
  if (style && style->has_text_shadow) {
    for (const auto& shadow : style->text_shadows) {
      if (shadow.color.a <= 0.0f) {
        continue;
      }
      const bool use_blur = shadow.blur_radius > 0.0f;
      if (use_blur) {
        commands.save();
        commands.set_blur(flex::BlurFilter(shadow.blur_radius));
      }
      draw_textarea_segmented(commands, text, x + shadow.offset_x,
                              y + shadow.offset_y, style, metrics,
                              shadow.color);
      if (use_blur) {
        commands.restore();
      }
    }
  }
  return draw_textarea_segmented(commands, text, x, y, style, metrics, color);
}

KeyCode remap_horizontal_key_for_direction(KeyCode key,
                                           const ComputedStyle* style) {
  if (!is_rtl(style)) {
    return key;
  }
  if (key == KeyCode::Left) {
    return KeyCode::Right;
  }
  if (key == KeyCode::Right) {
    return KeyCode::Left;
  }
  return key;
}

} // namespace

// ============================================================================
// 构造函数
// ============================================================================

TextAreaWidget::TextAreaWidget(const std::string& text, const std::string& placeholder)
    : text_(text), placeholder_(placeholder) {
  split_lines();
}

void TextAreaWidget::build_semantic_tree() {
  viewport_ = create_part("viewport", "viewport");
  selection_layer_ = create_part("selection-layer", "selection-layer");
  text_element_ = create_part("text", "text");
  placeholder_element_ = create_part("placeholder", "placeholder");
  caret_element_ = create_part("caret", "caret");
}

bool TextAreaWidget::paints_part_box(std::string_view part_name) const {
  return part_name == "selection-layer" || part_name == "text" ||
         part_name == "placeholder" || part_name == "caret";
}

bool TextAreaWidget::emit_part_render_commands(
    const Element& host, const Element& part, std::string_view part_name,
    RenderCommandList& commands) {
  (void)part;
  if (part_name == "selection-layer") {
    if (has_selection()) render_selection(commands, host, part.computed_style);
    return true;
  }
  if (part_name == "text") {
    if (!text_.empty()) render_text_lines(commands, host, part.computed_style);
    if (is_composing_) render_composition(commands, host);
    return true;
  }
  if (part_name == "placeholder") {
    if (text_.empty() && !placeholder_.empty() && !host.has_state("focus")) {
      render_placeholder(commands, host, part.computed_style);
    }
    return true;
  }
  if (part_name == "caret") {
    if (!is_composing_ && host.has_state("focus") && cursor_visible_) {
      render_cursor(commands, host, part.computed_style);
    }
    return true;
  }
  return false;
}

void TextAreaWidget::update_part_geometry(const Element& elem) {
  for (Element* part : {viewport_, selection_layer_, text_element_,
                        placeholder_element_, caret_element_}) {
    if (part) part->set_layout_bounds(0.0f, 0.0f, elem.width(), elem.height());
  }
  if (placeholder_element_) {
    placeholder_element_->set_visible(text_.empty() && !placeholder_.empty() &&
                                      !elem.has_state("focus"));
  }
  if (text_element_) text_element_->set_visible(!text_.empty() || is_composing_);
  if (selection_layer_) selection_layer_->set_visible(has_selection());
  if (caret_element_) {
    caret_element_->set_visible(!is_composing_ && elem.has_state("focus") &&
                                cursor_visible_);
  }
}

void TextAreaWidget::sync_host_semantics_for_layout(Element& elem) {
  sync_host_semantics();
  update_part_geometry(elem);
}

void TextAreaWidget::sync_host_semantics() {
  set_host_attribute("role", "textbox");
  set_host_boolean_attribute("aria-multiline", true);
  set_host_boolean_attribute("aria-readonly", readonly_);
  set_host_boolean_attribute("aria-disabled", disabled_);
  set_host_presence_attribute("readonly", readonly_);
  set_host_presence_attribute("disabled", disabled_);
  set_host_state("readonly", readonly_);
  set_host_state("disabled", disabled_);
  set_host_state("placeholder-shown", text_.empty() && !placeholder_.empty());
}

// ============================================================================
// 状态访问
// ============================================================================

void TextAreaWidget::set_text(const std::string& text) {
  if (text_ == text) return;
  text_ = text;
  split_lines();

  cursor_pos_ = clamp_utf8_offset(text_, cursor_pos_);
  text_input_common::clear_selection_state(selection_anchor_, selection_start_, selection_end_);
  undo_stack_.clear();
  redo_stack_.clear();
  invalidate_render_cache();
  sync_host_semantics();
}

TextValueObserverId TextAreaWidget::add_edit_observer(EditObserver observer) {
  if (!observer) {
    throw std::invalid_argument("TextAreaWidget edit observer must not be empty");
  }
  const TextValueObserverId id = next_edit_observer_id_++;
  edit_observers_.emplace_back(id, std::move(observer));
  return id;
}

bool TextAreaWidget::remove_edit_observer(TextValueObserverId id) {
  const auto it = std::find_if(
      edit_observers_.begin(), edit_observers_.end(),
      [id](const auto& entry) { return entry.first == id; });
  if (it == edit_observers_.end()) {
    return false;
  }
  edit_observers_.erase(it);
  return true;
}

void TextAreaWidget::notify_edit_observers() {
  const auto observers = edit_observers_;
  for (const auto& entry : observers) {
    if (entry.second) {
      entry.second(text_);
    }
  }
}

void TextAreaWidget::set_cursor_position(int pos) {
  const int clamped = clamp_utf8_offset(text_, pos);
  if (cursor_pos_ == clamped) {
    return;
  }
  cursor_pos_ = clamped;
  text_input_common::finish_cursor_change(cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
}

int TextAreaWidget::selection_start() const {
  if (!has_selection()) return cursor_pos_;
  return std::min(selection_start_, selection_end_);
}

int TextAreaWidget::selection_end() const {
  if (!has_selection()) return cursor_pos_;
  return std::max(selection_start_, selection_end_);
}

bool TextAreaWidget::has_selection() const {
  return selection_start_ >= 0 && selection_end_ >= 0 &&
         selection_start_ != selection_end_;
}

void TextAreaWidget::set_selection(int start, int end) {
  start = clamp_utf8_offset(text_, start);
  end = clamp_utf8_offset(text_, end);
  selection_anchor_ = start;
  selection_start_ = start;
  selection_end_ = end;
  cursor_pos_ = end;
  cursor_blink_time_ = 0.0f;
  cursor_visible_ = true;
  invalidate_render_cache();
}

void TextAreaWidget::clear_selection() {
  text_input_common::clear_selection_state(
      selection_anchor_, selection_start_, selection_end_);
  invalidate_render_cache();
}

int TextAreaWidget::position_from_line_column(int line, int column) const {
  if (lines_.empty()) return 0;
  line = std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1));
  column = clamp_utf8_offset(lines_[static_cast<size_t>(line)], column);
  int position = column;
  for (int current = 0; current < line; ++current) {
    position += static_cast<int>(lines_[static_cast<size_t>(current)].size()) + 1;
  }
  return position;
}

std::pair<int, int> TextAreaWidget::line_column_from_position(int position) const {
  position = clamp_utf8_offset(text_, position);
  int offset = 0;
  for (int line = 0; line < static_cast<int>(lines_.size()); ++line) {
    const int line_end = offset + static_cast<int>(lines_[static_cast<size_t>(line)].size());
    if (position <= line_end) {
      return {line, position - offset};
    }
    offset = line_end + 1;
  }
  return {static_cast<int>(lines_.size()) - 1,
          static_cast<int>(lines_.back().size())};
}

void TextAreaWidget::set_scroll_offset(float offset) {
  const float clamped = std::max(offset, 0.0f);
  if (scroll_offset_ == clamped) return;
  scroll_offset_ = clamped;
  invalidate_render_cache();
}

bool TextAreaWidget::measure_intrinsic_size(const Element& elem,
                                            float available_width,
                                            float available_height,
                                            float& out_width,
                                            float& out_height) const {
  (void)available_width;
  (void)available_height;
  const auto* style = elem.computed_style;
  const float font_size = style && style->font_size > 0.0f ? style->font_size : 16.0f;
  const auto metrics = resolve_editable_text_metrics(
      style, font_size, Symbol("--textarea-text"),
      style ? style->text_color : Color{0.1f, 0.1f, 0.1f, 1.0f},
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  float content_width = 0.0f;
  for (const auto& line : lines_) {
    content_width = std::max(
        content_width, textarea_line_width(line, style, metrics));
  }
  if (text_.empty()) {
    content_width = std::max(
        content_width, textarea_line_width(placeholder_, style, metrics));
  }

  const float padding_bottom = style ? style->padding[2] : 0.0f;

  out_width = std::max(content_width + metrics.padding_left +
                           metrics.padding_right,
                       font_size * 10.0f);
  out_height = std::max(
      static_cast<float>(std::max<size_t>(lines_.size(), 1)) *
              metrics.line_height +
          metrics.padding_top + padding_bottom,
      metrics.line_height + metrics.padding_top + padding_bottom);
  return true;
}

void TextAreaWidget::get_caret_rect(const Element& elem, float& x, float& y, float& w, float& h) const {
  auto* style = elem.computed_style;
  if (!style) return;

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  int line = get_line_from_cursor();
  int col = get_column_from_cursor();

  const std::string& current_line = lines_[static_cast<size_t>(
      std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1)))];
  x = textarea_caret_x(elem, style, metrics, current_line, col);
  y = metrics.padding_top + line * metrics.line_height - scroll_offset_;
  w = 2;
  h = metrics.line_height;
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void TextAreaWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  if (host_element() && is_semantic_tree_rendering()) {
    render_background(commands, elem);
    dirty_ = false;
    return;
  }
  if (render_cache_matches(elem)) {
    commands.append(render_cache_);
    dirty_ = false;
    return;
  }

  RenderCommandList* target = &commands;
  RenderCommandList rebuilt(commands.capabilities());
  if (can_use_render_cache(elem)) {
    target = &rebuilt;
  }

  render_background(*target, elem);

  if (text_.empty() && !placeholder_.empty() && !elem.has_state("focus")) {
    render_placeholder(*target, elem);
  } else {
    if (selection_start_ != -1 && selection_end_ != -1) {
      render_selection(*target, elem);
    }

    render_text_lines(*target, elem);

    if (is_composing_) {
      render_composition(*target, elem);
    } else if (elem.has_state("focus") && cursor_visible_) {
      render_cursor(*target, elem);
    }
  }

  if (target == &rebuilt) {
    render_cache_ = rebuilt.commands();
    update_render_cache_key(elem);
    commands.append(render_cache_);
  }

  dirty_ = false;
}

bool TextAreaWidget::can_use_render_cache(const Element& elem) const {
  return !elem.has_state("focus") && !is_composing_ && !is_dragging_ &&
         selection_start_ == -1 && selection_end_ == -1;
}

bool TextAreaWidget::render_cache_matches(const Element& elem) const {
  const auto* style = elem.computed_style;
  if (!render_cache_valid_ || !style || !can_use_render_cache(elem)) {
    return false;
  }

  return cached_text_ == text_ &&
         cached_placeholder_ == placeholder_ &&
         cached_disabled_state_ == (disabled_ || elem.has_state("disabled")) &&
         cached_readonly_state_ == (readonly_ || elem.has_state("readonly")) &&
         cached_style_signature_ == style->variables_signature() &&
         cached_width_ == elem.width() &&
         cached_height_ == elem.height() &&
         cached_opacity_ == style->opacity &&
         cached_font_size_ == style->font_size &&
         cached_letter_spacing_ == style->letter_spacing &&
         cached_word_spacing_ == style->word_spacing &&
         cached_tab_size_ == style->tab_size &&
         cached_scroll_offset_ == scroll_offset_ &&
         cached_font_weight_ == static_cast<int>(style->font_weight) &&
         cached_font_style_ == static_cast<int>(style->font_style) &&
         cached_direction_ == static_cast<int>(style->direction) &&
         cached_font_family_ == style->font_family &&
         cached_background_color_ == style->background_color &&
         cached_text_color_ == style->text_color &&
         cached_border_color_ == style->border_color;
}

void TextAreaWidget::update_render_cache_key(const Element& elem) {
  const auto* style = elem.computed_style;
  if (!style) {
    render_cache_valid_ = false;
    return;
  }

  cached_text_ = text_;
  cached_placeholder_ = placeholder_;
  cached_disabled_state_ = disabled_ || elem.has_state("disabled");
  cached_readonly_state_ = readonly_ || elem.has_state("readonly");
  cached_style_signature_ = style->variables_signature();
  cached_width_ = elem.width();
  cached_height_ = elem.height();
  cached_opacity_ = style->opacity;
  cached_font_size_ = style->font_size;
  cached_letter_spacing_ = style->letter_spacing;
  cached_word_spacing_ = style->word_spacing;
  cached_tab_size_ = style->tab_size;
  cached_scroll_offset_ = scroll_offset_;
  cached_font_weight_ = static_cast<int>(style->font_weight);
  cached_font_style_ = static_cast<int>(style->font_style);
  cached_direction_ = static_cast<int>(style->direction);
  cached_font_family_ = style->font_family;
  cached_background_color_ = style->background_color;
  cached_text_color_ = style->text_color;
  cached_border_color_ = style->border_color;
  render_cache_valid_ = true;
}

void TextAreaWidget::invalidate_render_cache() {
  render_cache_valid_ = false;
  dirty_ = true;
  if (auto* host = host_element()) {
    host->mark_paint_dirty();
  }
}

bool TextAreaWidget::handle_event(const Event& event, Element& elem) {
  if (disabled_ || elem.has_state("disabled")) {
    return false;
  }

  const bool was_placeholder_shown = text_.empty() && !placeholder_.empty();
  const std::string previous_text = text_;

  bool handled = false;
  switch (event.type) {
    case EventType::MouseDown: handled = handle_mouse_down(event, elem); break;
    case EventType::MouseMove: handled = handle_mouse_move(event, elem); break;
    case EventType::MouseUp: handled = handle_mouse_up(event, elem); break;
    case EventType::KeyDown: handled = handle_key_down(event, elem); break;
    case EventType::TextInput: handled = handle_text_input(event, elem); break;
    case EventType::CompositionStart: handled = handle_composition_start(event, elem); break;
    case EventType::CompositionUpdate: handled = handle_composition_update(event, elem); break;
    case EventType::CompositionEnd: handled = handle_composition_end(event, elem); break;
    case EventType::FocusIn:
      handled = text_input_common::handle_focus_in(cursor_blink_time_, cursor_visible_, elem);
      break;
    case EventType::FocusOut:
      elem.remove_state("focus");
      handled = text_input_common::handle_focus_out(
          is_composing_, composition_text_, cursor_blink_time_, cursor_visible_, elem);
      break;
    default:
      handled = false;
      break;
  }

  const bool is_placeholder_shown = text_.empty() && !placeholder_.empty();
  if (was_placeholder_shown != is_placeholder_shown) {
    sync_host_semantics();
  }
  if (text_ != previous_text) {
    notify_edit_observers();
  }
  return handled;
}

void TextAreaWidget::update(float delta_ms, Element& elem) {
  if (elem.has_state("focus") &&
      text_input_common::update_cursor_blink(
          true, delta_ms, 530.0f, cursor_blink_time_, cursor_visible_)) {
    dirty_ = true;
    elem.mark_paint_dirty();
  }
}

bool TextAreaWidget::needs_frame_update(const Element& elem) const {
  return is_dragging_ || is_composing_ || elem.has_state("focus");
}

// ============================================================================
// 文本处理
// ============================================================================

void TextAreaWidget::split_lines() {
  lines_.clear();

  std::stringstream ss(text_);
  std::string line;

  while (std::getline(ss, line, '\n')) {
    lines_.push_back(line);
  }

  // 如果文本以换行符结尾，添加空行
  if (!text_.empty() && text_.back() == '\n') {
    lines_.push_back("");
  }

  // 至少有一行（即使为空）
  if (lines_.empty()) {
    lines_.push_back("");
  }
}

void TextAreaWidget::rebuild_text() {
  std::string new_text;
  for (size_t i = 0; i < lines_.size(); i++) {
    new_text += lines_[i];
    if (i < lines_.size() - 1) {
      new_text += '\n';
    }
  }
  text_ = new_text;
}

int TextAreaWidget::get_line_from_cursor() const {
  int pos = 0;
  for (size_t i = 0; i < lines_.size(); i++) {
    int line_end = pos + static_cast<int>(lines_[i].size());
    if (cursor_pos_ <= line_end) {
      return static_cast<int>(i);
    }
    pos = line_end + 1;  // +1 for '\n'
  }
  return static_cast<int>(lines_.size()) - 1;
}

int TextAreaWidget::get_column_from_cursor() const {
  int pos = 0;
  int line = get_line_from_cursor();

  for (int i = 0; i < line; i++) {
    pos += static_cast<int>(lines_[i].size()) + 1;  // +1 for '\n'
  }

  return cursor_pos_ - pos;
}

void TextAreaWidget::move_cursor_to_line_column(int line, int col) {
  line = std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1));
  col = std::max(0, std::min(col, static_cast<int>(lines_[line].size())));

  int pos = 0;
  for (int i = 0; i < line; i++) {
    pos += static_cast<int>(lines_[i].size()) + 1;
  }
  pos += col;

  set_cursor_position(pos);
}

// ============================================================================
// 渲染辅助
// ============================================================================

void TextAreaWidget::render_background(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  const Color standard_background = !is_default_background(style->background_color)
                                        ? style->background_color
                                        : Color{1.0f, 1.0f, 1.0f, 1.0f};
  const Color standard_border = !is_default_border(style->border_color)
                                    ? style->border_color
                                    : Color{0.78f, 0.78f, 0.78f, 1.0f};
  const Color bg_color = style->get_variable_color(
      "--textarea-bg", standard_background);
  const Color border_color = style->get_variable_color(
      "--textarea-border", standard_border);
  const float radius = style->border_radius[0] > 0.0f
                           ? style->border_radius[0]
                           : 4.0f;
  const float css_border_width = *std::max_element(
      std::begin(style->border_width), std::end(style->border_width));

  commands.draw_rect(0, 0, elem.width(), elem.height(), radius, Paint::solid(bg_color),
                     Paint::none(), 0);

  const float stroke_width = elem.has_state("focus")
                                 ? std::max(css_border_width, 2.0f)
                                 : std::max(css_border_width, 1.0f);
  commands.draw_rect(0, 0, elem.width(), elem.height(), radius, Paint::none(),
                     Paint::solid(border_color), stroke_width);
}

void TextAreaWidget::render_text_lines(RenderCommandList& commands,
                                       const Element& elem,
                                       const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style) return;

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);
  const Color selection_text = style->get_variable_color(
      "--selection-color",
      style->get_variable_color("--textarea-selection-color", metrics.text_color));
  const Color text_color = part_style ? part_style->text_color : metrics.text_color;
  float y_top = metrics.padding_top;

  auto get_pos = [&](int p) -> std::pair<int, int> {
    int curr = 0;
    for (int i = 0; i < static_cast<int>(lines_.size()); i++) {
      int next = curr + static_cast<int>(lines_[i].size());
      if (p <= next) return {i, p - curr};
      curr = next + 1;
    }
    return {static_cast<int>(lines_.size()) - 1,
            static_cast<int>(lines_.back().size())};
  };

  int sel_start_line = -1;
  int sel_start_col = 0;
  int sel_end_line = -1;
  int sel_end_col = 0;
  if (selection_start_ != -1 && selection_end_ != -1 &&
      selection_start_ != selection_end_) {
    auto start = get_pos(std::min(selection_start_, selection_end_));
    auto end = get_pos(std::max(selection_start_, selection_end_));
    sel_start_line = start.first;
    sel_start_col = start.second;
    sel_end_line = end.first;
    sel_end_col = end.second;
  }

  for (size_t line_index = 0; line_index < lines_.size(); ++line_index) {
    const auto& line = lines_[line_index];
    if (y_top - scroll_offset_ > elem.height()) break;  // Over bottom
    if (y_top + metrics.line_height - scroll_offset_ < 0) {     // Above top
      y_top += metrics.line_height;
      continue;
    }

    if (!line.empty()) {
      float text_y = resolve_line_text_top(y_top, metrics);
      const float line_x = textarea_line_draw_x(elem, style, metrics, line);

      if (sel_start_line == -1 ||
          static_cast<int>(line_index) < sel_start_line ||
          static_cast<int>(line_index) > sel_end_line) {
        draw_textarea_segmented_with_shadows(
            commands, line, line_x, text_y - scroll_offset_, style, metrics,
            text_color);
      } else {
        const int local_start =
            static_cast<int>(line_index) == sel_start_line ? sel_start_col : 0;
        const int local_end =
            static_cast<int>(line_index) == sel_end_line ? sel_end_col
                                                         : static_cast<int>(line.size());

        const std::string before = line.substr(0, static_cast<size_t>(local_start));
        const std::string selected =
            line.substr(static_cast<size_t>(local_start),
                        static_cast<size_t>(std::max(local_end - local_start, 0)));
        const std::string after =
            line.substr(static_cast<size_t>(std::max(local_end, 0)));

        float current_x = line_x;
        if (!before.empty()) {
          current_x += draw_textarea_segmented_with_shadows(
              commands, before, current_x, text_y - scroll_offset_, style, metrics,
              text_color);
          if (!selected.empty() || !after.empty()) {
            current_x += effective_letter_spacing(style);
          }
        }
        if (!selected.empty()) {
          current_x += draw_textarea_segmented_with_shadows(
              commands, selected, current_x, text_y - scroll_offset_, style, metrics,
              selection_text);
          if (!after.empty()) {
            current_x += effective_letter_spacing(style);
          }
        }
        if (!after.empty()) {
          draw_textarea_segmented_with_shadows(
              commands, after, current_x, text_y - scroll_offset_, style,
              metrics, text_color);
        }
      }
    }

    y_top += metrics.line_height;
  }
}

void TextAreaWidget::render_placeholder(RenderCommandList& commands,
                                        const Element& elem,
                                        const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style) return;

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);
  const float y = resolve_line_text_top(metrics.padding_top, metrics);
  draw_textarea_segmented_with_shadows(
      commands, placeholder_, textarea_line_draw_x(elem, style, metrics, placeholder_),
      y, style, metrics,
      part_style ? part_style->text_color : metrics.placeholder_color);
}

void TextAreaWidget::render_selection(RenderCommandList& commands,
                                      const Element& elem,
                                      const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color sel_color = part_style
                        ? part_style->background_color
                        : style->get_variable_color(
                              "--selection-bg",
                              style->get_variable_color(
                                  "--textarea-selection-bg",
                                  {0.39f, 0.58f, 0.93f, 0.5f}));
  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  int start_pos = std::min(selection_start_, selection_end_);
  int end_pos = std::max(selection_start_, selection_end_);

  if (start_pos == -1 || end_pos == -1 || start_pos == end_pos) return;

  // Find start/end line and col
  auto get_pos = [&](int p) -> std::pair<int, int> {
    int curr = 0;
    for (int i = 0; i < (int)lines_.size(); i++) {
        int next = curr + (int)lines_[i].size();
        if (p <= next) return {i, p - curr};
        curr = next + 1; // +1 for \n
    }
    return {(int)lines_.size()-1, (int)lines_.back().size()};
  };

  auto start = get_pos(start_pos);
  auto end = get_pos(end_pos);

  for (int l = start.first; l <= end.first; l++) {
    int col_s = (l == start.first) ? start.second : 0;
    int col_e = (l == end.first) ? end.second : (int)lines_[l].size();

    float x0 = textarea_caret_x(elem, style, metrics, lines_[l], col_s);
    float x1 = textarea_caret_x(elem, style, metrics, lines_[l], col_e);
    float y = metrics.padding_top + l * metrics.line_height;
    float x = std::min(x0, x1);
    float w = std::fabs(x1 - x0);

    // If it's a multi-line selection and we're not on the last line, add a bit for the \n
    if (l < end.first) {
      w += textarea_line_width(" ", style, metrics) * 0.5f;
    }

    if (w > 0) {
      commands.draw_rect(x, y - scroll_offset_, w, metrics.line_height, 0,
                         Paint::solid(sel_color), Paint::none(), 0);
    }
  }
}

void TextAreaWidget::render_cursor(RenderCommandList& commands,
                                   const Element& elem,
                                   const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style) return;

  int line = get_line_from_cursor();
  int col = get_column_from_cursor();

  Color cursor_color = part_style
                           ? part_style->background_color
                           : style->get_variable_color(
                                 "--textarea-cursor",
                                 style->get_variable_color(
                                     "--caret-color",
                                     {0.0f, 0.0f, 0.0f, 1.0f}));
  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  const std::string& current_line = lines_[static_cast<size_t>(
      std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1)))];
  float x = textarea_caret_x(elem, style, metrics, current_line, col);
  float y = metrics.padding_top + line * metrics.line_height;

  commands.draw_rect(x, y - scroll_offset_, 2, metrics.line_height, 0,
                     Paint::solid(cursor_color), Paint::none(), 0);
}

// ============================================================================
// 事件处理
// ============================================================================

bool TextAreaWidget::handle_mouse_down(const Event& event, Element& elem) {
  elem.add_state("focus");

  auto* style = elem.computed_style;
  if (!style) return true;

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  flex::Vec2 local_pos =
      detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
  float rel_x = local_pos.x;
  float rel_y = local_pos.y;

  int line = static_cast<int>((rel_y - metrics.padding_top + scroll_offset_) / metrics.line_height);
  line = std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1));
  int col = textarea_column_from_x(elem, style, metrics, lines_[line], rel_x,
                                   static_cast<int>(lines_[line].size()));

  move_cursor_to_line_column(line, col);
  text_input_common::begin_drag_selection(
      cursor_pos_, selection_anchor_, selection_start_, selection_end_, is_dragging_);

  elem.mark_paint_dirty();
  return true;
}

bool TextAreaWidget::handle_mouse_move(const Event& event, Element& elem) {
  if (is_dragging_) {
    auto* style = elem.computed_style;
    if (!style) return true;
    const int old_cursor = cursor_pos_;
    const int old_selection_start = selection_start_;
    const int old_selection_end = selection_end_;

    const auto metrics = resolve_editable_text_metrics(
        style, style->font_size, Symbol("--textarea-text"), style->text_color,
        Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

    flex::Vec2 local_pos =
        detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
    float rel_x = local_pos.x;
    float rel_y = local_pos.y;

    int line = static_cast<int>((rel_y - metrics.padding_top + scroll_offset_) / metrics.line_height);
    line = std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1));
    int col = textarea_column_from_x(elem, style, metrics, lines_[line], rel_x,
                                     static_cast<int>(lines_[line].size()));

    move_cursor_to_line_column(line, col);
    text_input_common::update_drag_selection(
        cursor_pos_, selection_anchor_, selection_start_, selection_end_);

    if (cursor_pos_ != old_cursor || selection_start_ != old_selection_start ||
        selection_end_ != old_selection_end) {
      elem.mark_paint_dirty();
    }
    return true;
  }
  return false;
}

bool TextAreaWidget::handle_mouse_up(const Event& event, Element& elem) {
  if (is_dragging_) {
    text_input_common::finish_drag_selection(
        selection_anchor_, selection_start_, selection_end_, is_dragging_);
    elem.mark_paint_dirty();
    return true;
  }
  return false;
}

bool TextAreaWidget::handle_key_down(const Event& event, Element& elem) {
  bool shift = (event.mods & static_cast<int>(KeyMod::Shift)) != 0;
  bool ctrl = (event.mods & static_cast<int>(KeyMod::Control)) != 0;
  const KeyCode logical_key =
      remap_horizontal_key_for_direction(event.key, elem.computed_style);
  const bool readonly = readonly_ || elem.has_state("readonly");

  if (text_input_common::handle_copy_shortcut(ctrl, event.key, [&]() {
        copy_selection_to_clipboard();
        return true;
      })) {
    return true;
  }
  if (text_input_common::handle_cut_shortcut(
          ctrl, event.key, readonly, [&]() { return cut_selection_to_clipboard(); },
          [&]() { elem.mark_paint_dirty(); })) {
    return true;
  }
  if (text_input_common::handle_paste_shortcut(
          ctrl, event.key, readonly, [&]() { return paste_from_clipboard(); },
          [&]() { elem.mark_paint_dirty(); })) {
    return true;
  }

  if (readonly) {
    // 只读模式只允许导航
    switch (logical_key) {
      case KeyCode::Left:
      case KeyCode::Right:
      case KeyCode::Up:
      case KeyCode::Down:
      case KeyCode::Home:
      case KeyCode::End:
        break;
      default:
        return false;
    }
  }

  switch (logical_key) {
    case KeyCode::Backspace:
      if (text_input_common::has_selection(selection_start_, selection_end_)) {
        delete_selection();
      } else {
        delete_char_before_cursor();
      }
      return true;

    case KeyCode::Delete:
      if (text_input_common::has_selection(selection_start_, selection_end_)) {
        delete_selection();
      } else {
        delete_char_after_cursor();
      }
      return true;

    case KeyCode::Left:
      return text_input_common::handle_navigation(
          shift, cursor_pos_, selection_anchor_, selection_start_, selection_end_, [&]() {
            if (cursor_pos_ > 0) {
              set_cursor_position(previous_utf8_offset(text_, cursor_pos_));
            }
          });

    case KeyCode::Right:
      return text_input_common::handle_navigation(
          shift, cursor_pos_, selection_anchor_, selection_start_, selection_end_, [&]() {
            if (cursor_pos_ < static_cast<int>(text_.size())) {
              set_cursor_position(next_utf8_offset(text_, cursor_pos_));
            }
          });

    case KeyCode::Up: {
      return text_input_common::handle_navigation(
          shift, cursor_pos_, selection_anchor_, selection_start_, selection_end_, [&]() {
            int line = get_line_from_cursor();
            int col = get_column_from_cursor();
            if (line > 0) {
              move_cursor_to_line_column(line - 1, col);
            }
          });
    }

    case KeyCode::Down: {
      return text_input_common::handle_navigation(
          shift, cursor_pos_, selection_anchor_, selection_start_, selection_end_, [&]() {
            int line = get_line_from_cursor();
            int col = get_column_from_cursor();
            if (line < static_cast<int>(lines_.size()) - 1) {
              move_cursor_to_line_column(line + 1, col);
            }
          });
    }

    case KeyCode::Home: {
      return text_input_common::handle_navigation(
          shift, cursor_pos_, selection_anchor_, selection_start_, selection_end_, [&]() {
            int line = get_line_from_cursor();
            move_cursor_to_line_column(line, 0);
          });
    }

    case KeyCode::End: {
      return text_input_common::handle_navigation(
          shift, cursor_pos_, selection_anchor_, selection_start_, selection_end_, [&]() {
            int line = get_line_from_cursor();
            move_cursor_to_line_column(line, static_cast<int>(lines_[line].size()));
          });
    }

    case KeyCode::Enter:
      if (!readonly_ && !elem.has_state("readonly")) {
        insert_text("\n");
      }
      return true;

    case KeyCode::A:
      if (text_input_common::handle_select_all_shortcut(
              ctrl, logical_key,
              [&]() {
                text_input_common::select_all(
                    selection_anchor_, selection_start_, selection_end_, text_.size());
              },
              [&]() {
                text_input_common::finish_cursor_change(
                    cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
              })) {
        return true;
      }
      break;

    case KeyCode::Z:
    case KeyCode::Y:
      if (!readonly &&
          text_input_common::handle_undo_redo_shortcut(
              ctrl, shift, logical_key, [&]() { return undo_edit(); },
              [&]() { return redo_edit(); },
              [&]() {
                text_input_common::finish_text_change(
                    text_, change_callback_, cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
              })) {
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

bool TextAreaWidget::handle_text_input(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) return false;
  insert_text(event.text);
  return true;
}

std::string TextAreaWidget::selected_text() const {
  if (!text_input_common::has_selection(selection_start_, selection_end_)) {
    return {};
  }

  const auto [start, end] = text_input_common::selection_bounds(selection_start_, selection_end_);
  return text_.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
}

bool TextAreaWidget::copy_selection_to_clipboard() const {
  const std::string selected = selected_text();
  return text_input_common::copy_selection_to_clipboard(selected);
}

bool TextAreaWidget::cut_selection_to_clipboard() {
  if (!copy_selection_to_clipboard()) return false;
  delete_selection();
  return true;
}

bool TextAreaWidget::paste_from_clipboard() {
  std::string utf8;
  if (!text_input_common::read_clipboard_text(utf8)) return false;
  insert_text(utf8);
  return true;
}

void TextAreaWidget::insert_text(const std::string& str) {
  if (str.empty()) {
    return;
  }
  push_undo_snapshot();
  if (!text_input_common::insert_text_at_cursor(
          text_, str, cursor_pos_, selection_anchor_, selection_start_, selection_end_)) {
    return;
  }
  split_lines();

  text_input_common::finish_text_change(
      text_, change_callback_, cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
}

void TextAreaWidget::delete_selection() {
  if (!text_input_common::has_selection(selection_start_, selection_end_)) {
    return;
  }
  push_undo_snapshot();
  if (!text_input_common::erase_selected_text(
          text_, cursor_pos_, selection_anchor_, selection_start_, selection_end_)) {
    return;
  }
  split_lines();

  text_input_common::finish_text_change(
      text_, change_callback_, cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
}

void TextAreaWidget::delete_char_before_cursor() {
  if (cursor_pos_ <= 0) {
    return;
  }
  push_undo_snapshot();
  const int previous = previous_utf8_offset(text_, cursor_pos_);
  text_.erase(static_cast<size_t>(previous), static_cast<size_t>(cursor_pos_ - previous));
  cursor_pos_ = previous;
  split_lines();
  text_input_common::finish_text_change(
      text_, change_callback_, cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
}

void TextAreaWidget::delete_char_after_cursor() {
  if (cursor_pos_ >= static_cast<int>(text_.size())) {
    return;
  }
  push_undo_snapshot();
  const int next = next_utf8_offset(text_, cursor_pos_);
  text_.erase(static_cast<size_t>(cursor_pos_), static_cast<size_t>(next - cursor_pos_));
  split_lines();
  text_input_common::finish_text_change(
      text_, change_callback_, cursor_blink_time_, cursor_visible_, &dirty_, nullptr);
}

TextAreaWidget::EditSnapshot TextAreaWidget::capture_snapshot() const {
  return EditSnapshot{text_, cursor_pos_, selection_anchor_, selection_start_, selection_end_};
}

void TextAreaWidget::restore_snapshot(const EditSnapshot& snapshot) {
  text_ = snapshot.text;
  cursor_pos_ = snapshot.cursor_pos;
  selection_anchor_ = snapshot.selection_anchor;
  selection_start_ = snapshot.selection_start;
  selection_end_ = snapshot.selection_end;
  split_lines();
}

void TextAreaWidget::push_undo_snapshot() {
  const EditSnapshot snapshot = capture_snapshot();
  if (!undo_stack_.empty()) {
    const EditSnapshot& last = undo_stack_.back();
    if (last.text == snapshot.text && last.cursor_pos == snapshot.cursor_pos &&
        last.selection_anchor == snapshot.selection_anchor &&
        last.selection_start == snapshot.selection_start &&
        last.selection_end == snapshot.selection_end) {
      redo_stack_.clear();
      return;
    }
  }

  undo_stack_.push_back(snapshot);
  if (undo_stack_.size() > kMaxHistoryEntries) {
    undo_stack_.erase(undo_stack_.begin());
  }
  redo_stack_.clear();
}

bool TextAreaWidget::undo_edit() {
  if (undo_stack_.empty()) {
    return false;
  }

  redo_stack_.push_back(capture_snapshot());
  if (redo_stack_.size() > kMaxHistoryEntries) {
    redo_stack_.erase(redo_stack_.begin());
  }

  restore_snapshot(undo_stack_.back());
  undo_stack_.pop_back();
  return true;
}

bool TextAreaWidget::redo_edit() {
  if (redo_stack_.empty()) {
    return false;
  }

  undo_stack_.push_back(capture_snapshot());
  if (undo_stack_.size() > kMaxHistoryEntries) {
    undo_stack_.erase(undo_stack_.begin());
  }

  restore_snapshot(redo_stack_.back());
  redo_stack_.pop_back();
  return true;
}

// ============================================================================
// 动画更新
// ============================================================================

void TextAreaWidget::update_cursor_blink(float delta_ms) {
  if (text_input_common::update_cursor_blink(
          true, delta_ms, 530.0f, cursor_blink_time_, cursor_visible_)) {
    dirty_ = true;
  }
}

bool TextAreaWidget::handle_composition_start(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) return false;
  (void)event;
  return text_input_common::handle_composition_start(is_composing_, composition_text_, elem);
}

bool TextAreaWidget::handle_composition_update(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) return false;
  return text_input_common::handle_composition_update(
      is_composing_, composition_text_, event.composition_text, elem);
}

bool TextAreaWidget::handle_composition_end(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) return false;
  (void)event;
  return text_input_common::handle_composition_end(is_composing_, composition_text_, elem);
}

void TextAreaWidget::render_composition(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style || composition_text_.empty()) return;

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  int line = get_line_from_cursor();
  int col = get_column_from_cursor();

  const std::string& current_line = lines_[static_cast<size_t>(
      std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1)))];
  float x = textarea_caret_x(elem, style, metrics, current_line, col);
  float y_top = metrics.padding_top + line * metrics.line_height;
  float text_y = resolve_line_text_top(y_top, metrics);
  float w = textarea_line_width(composition_text_, style, metrics);
  if (is_rtl(style)) {
    x -= w;
  }

  Color text_color = style->get_variable_color("--textarea-text", style->text_color);
  draw_textarea_segmented_with_shadows(
      commands, composition_text_, x, text_y - scroll_offset_, style, metrics,
      text_color);

  commands.draw_rect(x, y_top + metrics.line_height - 2 - scroll_offset_, w, 2, 0,
                     Paint::solid(text_color), Paint::none(), 0);
}

} // namespace flexUI
