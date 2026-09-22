/*
 * flexUI - InputWidget Implementation
 *
 * Uses stb_textedit for robust text editing with undo/redo support.
 */

#include <flexUI/widgets/input_widget.h>
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
#include <stdexcept>
namespace flexUI {

namespace {

struct InputPalette {
  Color background;
  Color background_hover;
  Color text;
  Color border;
  Color border_focus;
  Color placeholder;
  Color selection;
  Color cursor;
  float border_width = 1.0f;
};

struct InputMetrics {
  float font_size = 14.0f;
  float padding_x = 12.0f;
  float padding_y = 9.0f;
  float radius = 8.0f;
};

bool approx_equal(float lhs, float rhs) {
  return std::fabs(lhs - rhs) <= 0.001f;
}

bool is_default_background(const Color& color) {
  return approx_equal(color.r, 1.0f) &&
         approx_equal(color.g, 1.0f) &&
         approx_equal(color.b, 1.0f) &&
         approx_equal(color.a, 0.0f);
}

bool is_default_text(const Color& color) {
  return approx_equal(color.r, 0.0f) &&
         approx_equal(color.g, 0.0f) &&
         approx_equal(color.b, 0.0f) &&
         approx_equal(color.a, 1.0f);
}

bool is_default_border(const Color& color) {
  return is_default_text(color);
}

bool has_any_padding(const ComputedStyle* style) {
  return style && (!approx_equal(style->padding[0], 0.0f) ||
                   !approx_equal(style->padding[1], 0.0f) ||
                   !approx_equal(style->padding[2], 0.0f) ||
                   !approx_equal(style->padding[3], 0.0f));
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
    const uint32_t cp = utf8_next_scalar(text, pos).value;
    if (is_word_spacing_gap(cp)) {
      ++count;
    }
  }
  return count;
}

size_t visual_index_to_byte_offset(const std::string& text, size_t visual_index) {
  size_t byte_pos = 0;
  size_t codepoint_index = 0;
  while (byte_pos < text.size() && codepoint_index < visual_index) {
    utf8_next_scalar(text, byte_pos).value;
    ++codepoint_index;
  }
  return byte_pos;
}

size_t byte_offset_to_visual_index(const std::string& text, size_t byte_offset) {
  const size_t clamped = std::min(byte_offset, text.size());
  size_t byte_pos = 0;
  size_t codepoint_index = 0;
  while (byte_pos < clamped) {
    utf8_next_scalar(text, byte_pos).value;
    ++codepoint_index;
  }
  return codepoint_index;
}

Color rgba_u8(unsigned char r, unsigned char g, unsigned char b,
              unsigned char a = 255) {
  return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

Color mix(const Color& lhs, const Color& rhs, float t) {
  return {
      lhs.r + (rhs.r - lhs.r) * t,
      lhs.g + (rhs.g - lhs.g) * t,
      lhs.b + (rhs.b - lhs.b) * t,
      lhs.a + (rhs.a - lhs.a) * t,
  };
}

InputPalette input_palette(InputWidget::Variant variant) {
  switch (variant) {
  case InputWidget::Variant::Filled:
    return {
        rgba_u8(241, 245, 249),
        rgba_u8(226, 232, 240),
        rgba_u8(15, 23, 42),
        rgba_u8(226, 232, 240),
        rgba_u8(59, 130, 246),
        rgba_u8(100, 116, 139),
        rgba_u8(59, 130, 246, 96),
        rgba_u8(15, 23, 42),
        1.0f,
    };
  case InputWidget::Variant::Ghost:
    return {
        rgba_u8(255, 255, 255, 0),
        rgba_u8(248, 250, 252),
        rgba_u8(15, 23, 42),
        rgba_u8(203, 213, 225, 0),
        rgba_u8(59, 130, 246),
        rgba_u8(100, 116, 139),
        rgba_u8(59, 130, 246, 96),
        rgba_u8(15, 23, 42),
        0.0f,
    };
  case InputWidget::Variant::Default:
  default:
    return {
        rgba_u8(255, 255, 255),
        rgba_u8(248, 250, 252),
        rgba_u8(15, 23, 42),
        rgba_u8(203, 213, 225),
        rgba_u8(59, 130, 246),
        rgba_u8(100, 116, 139),
        rgba_u8(59, 130, 246, 96),
        rgba_u8(15, 23, 42),
        1.0f,
    };
  }
}

InputMetrics input_metrics(InputWidget::Size size) {
  switch (size) {
  case InputWidget::Size::Small:
    return {13.0f, 10.0f, 7.0f, 6.0f};
  case InputWidget::Size::Large:
    return {16.0f, 14.0f, 11.0f, 10.0f};
  case InputWidget::Size::Default:
  default:
    return {14.0f, 12.0f, 9.0f, 8.0f};
  }
}

float effective_font_size(const InputWidget& widget, const ComputedStyle* style) {
  const auto metrics = input_metrics(widget.size());
  if (!style) return metrics.font_size;
  return style->get_variable_float(
      "--input-font-size",
      !approx_equal(style->font_size, 16.0f) ? style->font_size : metrics.font_size);
}

float effective_padding_left(const InputWidget& widget, const ComputedStyle* style) {
  const auto metrics = input_metrics(widget.size());
  if (!style) return metrics.padding_x;
  return style->get_variable_float(
      "--input-padding-x",
      has_any_padding(style) ? style->padding[3] : metrics.padding_x);
}

float effective_padding_right(const InputWidget& widget, const ComputedStyle* style) {
  const auto metrics = input_metrics(widget.size());
  if (!style) return metrics.padding_x;
  return style->get_variable_float(
      "--input-padding-x",
      has_any_padding(style) ? style->padding[1] : metrics.padding_x);
}

ComputedStyle make_input_measure_style(const ComputedStyle* style,
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

float measure_input_text_width(const std::string& text, const ComputedStyle* style,
                               const EditableTextMetrics& metrics) {
  const ComputedStyle measure_style = make_input_measure_style(style, metrics);
  const std::string display = transform_text_for_layout(style, text);
  float width = 0.0f;
  size_t codepoint_count = 0;
  size_t word_gap_count = 0;
  for (const auto& segment : segment_text(display)) {
    if (segment.type == TextSegmentType::Emoji) {
      const size_t count = utf8_scalar_count(segment.text);
      width += static_cast<float>(count) * metrics.font_size;
      codepoint_count += count;
      continue;
    }

    const float segment_width = approximate_text_width(&measure_style, segment.text);
    width += segment_width > 0.0f
                 ? segment_width
                 : metrics.char_width * static_cast<float>(utf8_scalar_count(segment.text));
    const size_t count = utf8_scalar_count(segment.text);
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

float measure_input_codepoint_width(const std::string& glyph, uint32_t cp,
                                    const ComputedStyle* style,
                                    const EditableTextMetrics& metrics) {
  if (is_emoji(cp)) {
    return metrics.font_size;
  }

  const ComputedStyle measure_style = make_input_measure_style(style, metrics);
  const std::string display = transform_text_for_layout(style, glyph);
  const float glyph_width = approximate_text_width(&measure_style, display);
  return glyph_width > 0.0f ? glyph_width : metrics.char_width;
}

float input_segment_width(const TextSegment& segment, const ComputedStyle* style,
                          const EditableTextMetrics& metrics) {
  if (segment.type == TextSegmentType::Emoji) {
    size_t count = 0;
    size_t pos = 0;
    while (pos < segment.text.size()) {
      utf8_next_scalar(segment.text, pos).value;
      count++;
    }
    return static_cast<float>(count) * metrics.font_size;
  }
  return measure_input_text_width(segment.text, style, metrics);
}

float draw_input_segmented(RenderCommandList& commands, const std::string& text, float x, float y,
                           const ComputedStyle* style,
                           const EditableTextMetrics& metrics, const Color& color) {
  float current_x = x;
  const float letter_spacing = effective_letter_spacing(style);
  const float word_spacing = effective_word_spacing(style);
  const ComputedStyle measure_style = make_input_measure_style(style, metrics);
  const std::string display = transform_text_for_layout(style, text);
  const auto segments = segment_text(display);
  const bool bold = style && style->font_weight >= FontWeight::Bold;
  for (size_t i = 0; i < segments.size(); ++i) {
    const auto& seg = segments[i];
    const std::string font_name =
        seg.type == TextSegmentType::Emoji ? get_emoji_font_name()
                                           : metrics.font_family;
    const bool has_tab = seg.text.find('\t') != std::string::npos;
    if (seg.type == TextSegmentType::Emoji ||
        (letter_spacing <= 0.0f && word_spacing <= 0.0f && !has_tab)) {
      commands.draw_text(seg.text, current_x, y, font_name, metrics.font_size,
                         bold, color);
      current_x += input_segment_width(seg, style, metrics);
    } else if (letter_spacing <= 0.0f) {
      size_t byte_pos = 0;
      while (byte_pos < seg.text.size()) {
        const size_t run_start = byte_pos;
        const uint32_t first_cp = utf8_next_scalar(seg.text, byte_pos).value;
        const bool gap_run = is_word_spacing_gap(first_cp);
        while (byte_pos < seg.text.size()) {
          const size_t before = byte_pos;
          const uint32_t cp = utf8_next_scalar(seg.text, byte_pos).value;
          if (is_word_spacing_gap(cp) != gap_run) {
            byte_pos = before;
            break;
          }
        }
        const std::string run = seg.text.substr(run_start, byte_pos - run_start);
        const float run_width = approximate_text_width(&measure_style, run);
        if (!gap_run) {
          commands.draw_text(run, current_x, y, font_name, metrics.font_size,
                             bold, color);
        }
        current_x += run_width > 0.0f
                         ? run_width
                         : metrics.char_width *
                               static_cast<float>(utf8_scalar_count(run));
        current_x += word_spacing *
                     static_cast<float>(count_word_spacing_gaps(run));
      }
    } else {
      size_t byte_pos = 0;
      while (byte_pos < seg.text.size()) {
        const size_t start = byte_pos;
        const uint32_t cp = utf8_next_scalar(seg.text, byte_pos).value;
        const std::string glyph = seg.text.substr(start, byte_pos - start);
        const float glyph_width = approximate_text_width(&measure_style, glyph);
        if (!is_word_spacing_gap(cp)) {
          commands.draw_text(glyph, current_x, y, font_name, metrics.font_size,
                             bold, color);
        }
        current_x += glyph_width > 0.0f ? glyph_width : metrics.char_width;
        if (is_word_spacing_gap(cp)) {
          current_x += word_spacing;
        }
        if (byte_pos < seg.text.size()) {
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

float draw_input_segmented_with_shadows(
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
      draw_input_segmented(commands, text, x + shadow.offset_x,
                           y + shadow.offset_y, style, metrics, shadow.color);
      if (use_blur) {
        commands.restore();
      }
    }
  }
  return draw_input_segmented(commands, text, x, y, style, metrics, color);
}

float input_content_right(const InputWidget& widget, const Element& elem,
                          const ComputedStyle* style) {
  return elem.width() - effective_padding_right(widget, style);
}

float input_text_origin_x(const InputWidget& widget, const Element& elem,
                          const ComputedStyle* style,
                          const EditableTextMetrics& metrics,
                          const std::string& display) {
  if (!is_rtl(style)) {
    return effective_padding_left(widget, style);
  }
  return input_content_right(widget, elem, style) -
         measure_input_text_width(display, style, metrics);
}

float effective_radius(const InputWidget& widget, const ComputedStyle* style) {
  const auto metrics = input_metrics(widget.size());
  if (!style) return metrics.radius;
  return style->get_variable_float(
      "--input-radius",
      style->border_radius[0] > 0.0f ? style->border_radius[0] : metrics.radius);
}

} // namespace

// ============================================================================
// 构造函数
// ============================================================================

InputWidget::InputWidget(const std::string& placeholder, bool password)
    : placeholder_(placeholder), password_(password) {
  textedit_ = std::make_unique<TextEdit>();
}

void InputWidget::build_semantic_tree() {
  viewport_ = create_part("viewport", "viewport");
  selection_layer_ = create_part("selection-layer", "selection-layer");
  text_element_ = create_part("text", "text");
  placeholder_element_ = create_part("placeholder", "placeholder");
  caret_element_ = create_part("caret", "caret");
}

bool InputWidget::paints_part_box(std::string_view part_name) const {
  return part_name == "selection-layer" || part_name == "text" ||
         part_name == "placeholder" || part_name == "caret";
}

bool InputWidget::emit_part_render_commands(
    const Element& host, const Element& part, std::string_view part_name,
    RenderCommandList& commands) {
  (void)part;
  if (part_name == "selection-layer") {
    if (has_selection()) render_selection(commands, host, part.computed_style);
    return true;
  }
  if (part_name == "text") {
    if (!text_.empty()) render_text(commands, host, part.computed_style);
    if (is_composing_) render_composition(commands, host);
    return true;
  }
  if (part_name == "placeholder") {
    if (text_.empty() && !placeholder_.empty() && !host.has_state("focus")) {
      render_text(commands, host, part.computed_style);
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

void InputWidget::update_part_geometry(const Element& elem) {
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

void InputWidget::sync_host_semantics_for_layout(Element& elem) {
  sync_host_semantics();
  ensure_textedit_init(elem);
  update_part_geometry(elem);
}

bool InputWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                         float available_height, float& out_width,
                                         float& out_height) const {
  (void)available_width;
  (void)available_height;
  const auto* style = elem.computed_style;
  const auto metrics = input_metrics(size_);
  const float font_size = effective_font_size(*this, style);

  ComputedStyle measure_style;
  if (style) {
    measure_style = *style;
  }
  measure_style.font_size = font_size;

  const std::string display = !display_text().empty() ? display_text() : placeholder_;
  const float text_width = display.empty()
                               ? font_size * 4.0f
                               : approximate_segmented_text_width(&measure_style, display);
  const float padding_x = has_any_padding(style)
                              ? style->padding[1] + style->padding[3]
                              : metrics.padding_x * 2.0f;
  const float padding_y = has_any_padding(style)
                              ? style->padding[0] + style->padding[2]
                              : metrics.padding_y * 2.0f;
  out_width = std::max(text_width + padding_x + 2.0f, 160.0f);
  out_height = std::max(font_size + padding_y, font_size + 18.0f);
  return true;
}

void InputWidget::sync_host_semantics() {
  set_host_attribute("role", "textbox");
  set_host_boolean_attribute("aria-readonly", readonly_);
  set_host_boolean_attribute("aria-disabled", disabled_);
  set_host_presence_attribute("readonly", readonly_);
  set_host_presence_attribute("disabled", disabled_);
  set_host_state("readonly", readonly_);
  set_host_state("disabled", disabled_);
  set_host_state("placeholder-shown", text_.empty() && !placeholder_.empty());
}

// ============================================================================
// TextEdit 初始化（延迟）
// ============================================================================

void InputWidget::ensure_textedit_init(const Element& elem) {
  if (textedit_initialized_) return;

  auto* style = elem.computed_style;
  const auto metrics = resolve_editable_text_metrics(
      style, effective_font_size(*this, style), Symbol("--input-text"),
      input_palette(variant_).text, Symbol("--input-placeholder"),
      input_palette(variant_).placeholder, 1.0f);

  textedit_->init(&text_, metrics.char_width, metrics.line_height, false);
  textedit_initialized_ = true;
}

// ============================================================================
// 文本访问
// ============================================================================

void InputWidget::set_text(const std::string& text) {
  if (text_ != text) {
    text_ = text;
    // Re-init textedit with new text
    textedit_initialized_ = false;
    invalidate_render_cache();
    sync_host_semantics();
  }
}

TextValueObserverId InputWidget::add_edit_observer(EditObserver observer) {
  if (!observer) {
    throw std::invalid_argument("InputWidget edit observer must not be empty");
  }
  const TextValueObserverId id = next_edit_observer_id_++;
  edit_observers_.emplace_back(id, std::move(observer));
  return id;
}

bool InputWidget::remove_edit_observer(TextValueObserverId id) {
  const auto it = std::find_if(
      edit_observers_.begin(), edit_observers_.end(),
      [id](const auto& entry) { return entry.first == id; });
  if (it == edit_observers_.end()) {
    return false;
  }
  edit_observers_.erase(it);
  return true;
}

void InputWidget::notify_edit_observers() {
  const auto observers = edit_observers_;
  for (const auto& entry : observers) {
    if (entry.second) {
      entry.second(text_);
    }
  }
}

void InputWidget::set_cursor_pos(size_t pos) {
  if (textedit_) {
    textedit_->set_cursor(
        static_cast<int>(visual_index_to_byte_offset(display_text(), pos)));
  }
  dirty_ = true;
}

size_t InputWidget::cursor_pos() const {
  return textedit_ ? byte_offset_to_visual_index(
                         display_text(), static_cast<size_t>(textedit_->cursor()))
                   : 0;
}

void InputWidget::get_caret_rect(const Element& elem, float& x, float& y, float& w, float& h) const {
  auto* style = elem.computed_style;
  if (!style) return;

  const int cursor_pos = textedit_ ? textedit_->cursor() : 0;
  const float font_size = effective_font_size(*this, style);
  x = index_to_x(static_cast<size_t>(cursor_pos), elem);
  h = font_size;
  y = (elem.height() - h) / 2.0f;
  w = 1.0f;
}

std::string InputWidget::selected_text() const {
  if (textedit_) {
    return textedit_->selected_text();
  }
  return "";
}

void InputWidget::select_all() {
  if (textedit_) {
    textedit_->select_all();
  }
  invalidate_render_cache();
}

void InputWidget::clear_selection() {
  if (textedit_) {
    textedit_->clear_selection();
  }
  invalidate_render_cache();
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void InputWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  if (host_element() && is_semantic_tree_rendering()) {
    ensure_textedit_init(elem);
    render_background(commands, elem);
    dirty_ = false;
    return;
  }
  if (render_cache_matches(elem)) {
    commands.append(render_cache_);
    dirty_ = false;
    return;
  }

  // Ensure textedit is initialized for cursor/selection queries
  const_cast<InputWidget*>(this)->ensure_textedit_init(elem);

  RenderCommandList* target = &commands;
  RenderCommandList rebuilt(commands.capabilities());
  if (can_use_render_cache(elem)) {
    target = &rebuilt;
  }

  render_background(*target, elem);

  if (has_selection()) {
    render_selection(*target, elem);
  }

  render_text(*target, elem);

  // 只有聚焦时才显示光标
  if (is_composing_) {
    render_composition(*target, elem);
  } else if (elem.has_state("focus") && cursor_visible_) {
    render_cursor(*target, elem);
  }

  if (target == &rebuilt) {
    render_cache_ = rebuilt.commands();
    update_render_cache_key(elem);
    commands.append(render_cache_);
  }

  dirty_ = false;
}

bool InputWidget::can_use_render_cache(const Element& elem) const {
  return !elem.has_state("focus") && !is_composing_ && !is_dragging_ &&
         !has_selection();
}

bool InputWidget::render_cache_matches(const Element& elem) const {
  const auto* style = elem.computed_style;
  if (!render_cache_valid_ || !style || !can_use_render_cache(elem)) {
    return false;
  }

  return cached_text_ == text_ &&
         cached_placeholder_ == placeholder_ &&
         cached_hover_state_ == elem.has_state("hover") &&
         cached_disabled_state_ == (disabled_ || elem.has_state("disabled")) &&
         cached_readonly_state_ == (readonly_ || elem.has_state("readonly")) &&
         cached_password_ == password_ &&
         cached_variant_ == static_cast<int>(variant_) &&
         cached_size_ == static_cast<int>(size_) &&
         cached_style_signature_ == style->variables_signature() &&
         cached_width_ == elem.width() &&
         cached_height_ == elem.height() &&
         cached_opacity_ == style->opacity &&
         cached_font_size_ == style->font_size &&
         cached_letter_spacing_ == style->letter_spacing &&
         cached_word_spacing_ == style->word_spacing &&
         cached_tab_size_ == style->tab_size &&
         cached_padding_[0] == style->padding[0] &&
         cached_padding_[1] == style->padding[1] &&
         cached_padding_[2] == style->padding[2] &&
         cached_padding_[3] == style->padding[3] &&
         cached_border_radius_ == style->border_radius[0] &&
         cached_font_weight_ == static_cast<int>(style->font_weight) &&
         cached_font_style_ == static_cast<int>(style->font_style) &&
         cached_direction_ == static_cast<int>(style->direction) &&
         cached_font_family_ == style->font_family &&
         cached_background_color_ == style->background_color &&
         cached_text_color_ == style->text_color &&
         cached_border_color_ == style->border_color;
}

void InputWidget::update_render_cache_key(const Element& elem) {
  const auto* style = elem.computed_style;
  if (!style) {
    render_cache_valid_ = false;
    return;
  }

  cached_text_ = text_;
  cached_placeholder_ = placeholder_;
  cached_hover_state_ = elem.has_state("hover");
  cached_disabled_state_ = disabled_ || elem.has_state("disabled");
  cached_readonly_state_ = readonly_ || elem.has_state("readonly");
  cached_password_ = password_;
  cached_variant_ = static_cast<int>(variant_);
  cached_size_ = static_cast<int>(size_);
  cached_style_signature_ = style->variables_signature();
  cached_width_ = elem.width();
  cached_height_ = elem.height();
  cached_opacity_ = style->opacity;
  cached_font_size_ = style->font_size;
  cached_letter_spacing_ = style->letter_spacing;
  cached_word_spacing_ = style->word_spacing;
  cached_tab_size_ = style->tab_size;
  cached_padding_[0] = style->padding[0];
  cached_padding_[1] = style->padding[1];
  cached_padding_[2] = style->padding[2];
  cached_padding_[3] = style->padding[3];
  cached_border_radius_ = style->border_radius[0];
  cached_font_weight_ = static_cast<int>(style->font_weight);
  cached_font_style_ = static_cast<int>(style->font_style);
  cached_direction_ = static_cast<int>(style->direction);
  cached_font_family_ = style->font_family;
  cached_background_color_ = style->background_color;
  cached_text_color_ = style->text_color;
  cached_border_color_ = style->border_color;
  render_cache_valid_ = true;
}

void InputWidget::invalidate_render_cache() {
  render_cache_valid_ = false;
  dirty_ = true;
  if (auto* host = host_element()) {
    host->mark_paint_dirty();
  }
}

bool InputWidget::handle_event(const Event& event, Element& elem) {
  if (disabled_ || elem.has_state("disabled")) {
    return false;
  }

  ensure_textedit_init(elem);
  const std::string previous_text = text_;
  const bool was_placeholder_shown = text_.empty() && !placeholder_.empty();

  bool handled = false;
  switch (event.type) {
    case EventType::KeyDown:
      handled = handle_key_down(event, elem);
      break;

    case EventType::TextInput:
      handled = handle_text_input(event, elem);
      break;

    case EventType::FocusIn:
      handled =
          text_input_common::handle_focus_in(cursor_blink_time_, cursor_visible_, elem);
      break;

    case EventType::FocusOut:
      handled = text_input_common::handle_focus_out(
          is_composing_, composition_text_, cursor_blink_time_, cursor_visible_, elem);
      break;

    case EventType::CompositionStart:
      handled = handle_composition_start(event, elem);
      break;

    case EventType::CompositionUpdate:
      handled = handle_composition_update(event, elem);
      break;

    case EventType::CompositionEnd:
      handled = handle_composition_end(event, elem);
      break;

    case EventType::MouseDown:
      handled = handle_mouse_down(event, elem);
      break;

    case EventType::MouseMove:
      handled = handle_mouse_move(event, elem);
      break;

    case EventType::MouseUp:
      handled = handle_mouse_up(event, elem);
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

void InputWidget::update(float delta_ms, Element& elem) {
  if (text_input_common::update_cursor_blink(
          elem.has_state("focus"), delta_ms, 500.0f, cursor_blink_time_, cursor_visible_)) {
    elem.mark_paint_dirty();
  }
}

bool InputWidget::needs_frame_update(const Element& elem) const {
  return is_dragging_ || is_composing_ || elem.has_state("focus");
}

// ============================================================================
// 渲染辅助
// ============================================================================

void InputWidget::render_background(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  const auto palette = input_palette(variant_);
  const bool has_css_background =
      !style->get_variable("--input-bg", "").empty() ||
      !style->get_variable("--bg", "").empty() ||
      !is_default_background(style->background_color);
  const bool has_css_border =
      !style->get_variable("--input-border", "").empty() ||
      !style->get_variable("--border-color", "").empty() ||
      !style->get_variable("--border", "").empty() ||
      !is_default_border(style->border_color);

  float radius = effective_radius(*this, style);
  Color bg_color = style->get_variable_color(
      "--input-bg", style->get_variable_color("--bg",
      has_css_background ? style->background_color : palette.background));
  Color hover_bg = style->get_variable_color(
      "--input-bg-hover",
      has_css_background ? mix(bg_color, Color{1.0f, 1.0f, 1.0f, bg_color.a}, 0.06f)
                         : palette.background_hover);
  Color border_color = style->get_variable_color(
      "--input-border",
      style->get_variable_color("--border-color",
      style->get_variable_color("--border",
      has_css_border ? style->border_color : palette.border)));
  Color focus_border = style->get_variable_color("--input-border-focus",
                                                 palette.border_focus);
  float border_width = style->get_variable_float("--input-border-width",
                                                 has_css_border ? 1.0f : palette.border_width);

  if (elem.has_state("hover")) {
    bg_color = hover_bg;
  }

  if (elem.has_state("focus")) {
    border_color = focus_border;
    border_width = std::max(border_width, 1.5f);
  }

  if (elem.has_state("disabled")) {
    bg_color = mix(bg_color, Color{1.0f, 1.0f, 1.0f, bg_color.a}, 0.3f);
    border_color.a *= 0.75f;
  }

  commands.draw_rect(0, 0, elem.width(), elem.height(), radius,
                     Paint::solid(bg_color), Paint::none(), 0);

  commands.draw_rect(0, 0, elem.width(), elem.height(), radius,
                     Paint::none(), Paint::solid(border_color), border_width);
}

void InputWidget::render_text(RenderCommandList& commands, const Element& elem,
                              const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style) return;

  std::string display = display_text();
  const auto palette = input_palette(variant_);
  const bool has_css_text =
      !style->get_variable("--input-text", "").empty() ||
      !style->get_variable("--text-color", "").empty() ||
      !is_default_text(style->text_color);
  const auto metrics = resolve_editable_text_metrics(
      style, effective_font_size(*this, style), Symbol("--input-text"),
      has_css_text ? style->text_color : palette.text,
      Symbol("--input-placeholder"),
      has_css_text ? style->text_color : palette.placeholder, 1.0f);
  const float text_y = (elem.height() - metrics.font_size) / 2;

  if (display.empty() && !placeholder_.empty()) {
    float current_x = input_text_origin_x(*this, elem, style, metrics, placeholder_);
    draw_input_segmented_with_shadows(commands, placeholder_, current_x, text_y,
                                      style, metrics,
                                      part_style ? part_style->text_color
                                                 : metrics.placeholder_color);
    return;
  }

  if (!display.empty()) {
    const int selection_start = textedit_ ? textedit_->selection_start() : 0;
    const int selection_end = textedit_ ? textedit_->selection_end() : 0;
    const int sel_from = std::min(selection_start, selection_end);
    const int sel_to = std::max(selection_start, selection_end);
    const bool has_selection =
        textedit_ && selection_start != selection_end;
    const Color selection_text = style->get_variable_color(
        "--selection-color",
        style->get_variable_color("--input-selection-color", metrics.text_color));
    float current_x = input_text_origin_x(*this, elem, style, metrics, display);
    const float boundary_spacing = effective_letter_spacing(style);
    const auto draw_span = [&](const std::string& span, const Color& color,
                               bool add_boundary_spacing) {
      if (span.empty()) {
        return;
      }
      current_x += draw_input_segmented_with_shadows(
          commands, span, current_x, text_y, style, metrics, color);
      if (add_boundary_spacing) {
        current_x += boundary_spacing;
      }
    };

    if (!has_selection) {
      draw_span(display, part_style ? part_style->text_color : metrics.text_color,
                false);
      return;
    }

    const std::string before = display.substr(0, static_cast<size_t>(sel_from));
    const std::string selected =
        display.substr(static_cast<size_t>(sel_from),
                       static_cast<size_t>(sel_to - sel_from));
    const std::string after = display.substr(static_cast<size_t>(sel_to));
    const Color text_color = part_style ? part_style->text_color : metrics.text_color;
    draw_span(before, text_color, !before.empty() && (!selected.empty() || !after.empty()));
    draw_span(selected, selection_text, !selected.empty() && !after.empty());
    draw_span(after, text_color, false);
  }
}

namespace {

KeyCode remap_horizontal_key_for_direction(KeyCode key, const ComputedStyle* style) {
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

void InputWidget::render_cursor(RenderCommandList& commands, const Element& elem,
                                const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style) return;

  int cursor_pos = textedit_ ? textedit_->cursor() : 0;
  const auto palette = input_palette(variant_);
  const auto metrics = resolve_editable_text_metrics(
      style, effective_font_size(*this, style), Symbol("--input-text"),
      palette.text, Symbol("--input-placeholder"), palette.placeholder, 1.0f);
  float cursor_x = index_to_x(cursor_pos, elem);
  float cursor_h = metrics.font_size;
  float cursor_y = (elem.height() - cursor_h) / 2;

  Color cursor_color = part_style
                           ? part_style->background_color
                           : style->get_variable_color(
                                 "--input-cursor",
                                 style->get_variable_color("--caret-color",
                                                           palette.cursor));

  commands.draw_rect(cursor_x, cursor_y, 1, cursor_h, 0, Paint::solid(cursor_color),
                     Paint::none(), 0);
}

void InputWidget::render_selection(RenderCommandList& commands,
                                   const Element& elem,
                                   const ComputedStyle* part_style) {
  auto* style = elem.computed_style;
  if (!style || !textedit_) return;

  int start = textedit_->selection_start();
  int end = textedit_->selection_end();

  float start_x = index_to_x(start, elem);
  float end_x = index_to_x(end, elem);
  const auto palette = input_palette(variant_);

  Color selection_bg = part_style
                           ? part_style->background_color
                           : style->get_variable_color(
                                 "--selection-bg",
                                 style->get_variable_color(
                                     "--input-selection-bg", palette.selection));

  commands.draw_rect(std::min(start_x, end_x), 0.0f, std::fabs(end_x - start_x),
                     elem.height(), 0, Paint::solid(selection_bg), Paint::none(), 0);
}

void InputWidget::render_composition(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style || composition_text_.empty()) return;

  const auto palette = input_palette(variant_);
  const auto metrics = resolve_editable_text_metrics(
      style, effective_font_size(*this, style), Symbol("--input-text"),
      palette.text, Symbol("--input-placeholder"), palette.placeholder, 1.0f);
  const int cursor_pos = textedit_ ? textedit_->cursor() : 0;
  const float x = index_to_x(static_cast<size_t>(cursor_pos), elem);
  const float y = (elem.height() - metrics.font_size) / 2.0f;
  const float underline_y = y + metrics.font_size + 1.0f;
  const float width = measure_input_text_width(composition_text_, style, metrics);
  Color text_color = style->get_variable_color("--input-text", palette.text);

  draw_input_segmented_with_shadows(commands, composition_text_, x, y, style,
                                    metrics, text_color);
  commands.draw_rect(x, underline_y, width, 1.0f, 0.0f, Paint::solid(text_color),
                     Paint::none(), 0.0f);
}

// ============================================================================
// 事件处理
// ============================================================================

bool InputWidget::handle_key_down(const Event& event, Element& elem) {
  bool shift = (event.mods & static_cast<int>(KeyMod::Shift)) != 0;
  bool ctrl = (event.mods & static_cast<int>(KeyMod::Control)) != 0;
  const KeyCode logical_key =
      remap_horizontal_key_for_direction(event.key, elem.computed_style);
  const bool readonly = readonly_ || elem.has_state("readonly");

  // Try stb_textedit key handling first
  int stb_key = 0;
  if (!readonly || logical_key == KeyCode::Left || logical_key == KeyCode::Right ||
      logical_key == KeyCode::Home || logical_key == KeyCode::End) {
    stb_key = map_key_to_stb(static_cast<int>(logical_key), shift, ctrl);
  }
  if (stb_key != 0 && textedit_) {
    textedit_->key(static_cast<int>(logical_key), shift, ctrl);

    text_input_common::finish_text_change(
        text_, change_callback_, cursor_blink_time_, cursor_visible_, nullptr, &elem);
    return true;
  }

  // Handle special keys not in stb_textedit
  switch (event.key) {
    case KeyCode::A:
      if (textedit_ &&
          text_input_common::handle_select_all_shortcut(
              ctrl, event.key, [&]() { textedit_->select_all(); },
              [&]() { elem.mark_paint_dirty(); })) {
        return true;
      }
      break;

    case KeyCode::C:
      if (textedit_ &&
          text_input_common::handle_copy_shortcut(
              ctrl, event.key,
              [&]() { return has_selection() && text_input_common::copy_selection_to_clipboard(textedit_->copy()); })) {
        return true;
      }
      break;

    case KeyCode::X:
      if (textedit_ &&
          text_input_common::handle_cut_shortcut(
              ctrl, event.key, readonly,
              [&]() {
                if (!has_selection()) {
                  return false;
                }
                return text_input_common::copy_selection_to_clipboard(textedit_->cut());
              },
              [&]() {
                text_input_common::finish_text_change(
                    text_, change_callback_, cursor_blink_time_, cursor_visible_, nullptr, &elem);
              })) {
        return true;
      }
      break;

    case KeyCode::V:
      if (textedit_ &&
          text_input_common::handle_paste_shortcut(
              ctrl, event.key, readonly,
              [&]() {
                std::string text;
                if (!text_input_common::read_clipboard_text(text)) {
                  return false;
                }
                textedit_->paste(text);
                return true;
              },
              [&]() {
                text_input_common::finish_text_change(
                    text_, change_callback_, cursor_blink_time_, cursor_visible_, nullptr, &elem);
              })) {
        return true;
      }
      break;

    case KeyCode::Z:
      if (textedit_ && !readonly &&
          text_input_common::handle_undo_redo_shortcut(
              ctrl, shift, event.key, [&]() { textedit_->undo(); },
              [&]() { textedit_->redo(); },
              [&]() {
                text_input_common::finish_text_change(
                    text_, change_callback_, cursor_blink_time_, cursor_visible_, nullptr, &elem);
              })) {
        return true;
      }
      break;

    case KeyCode::Y:
      if (textedit_ && !readonly &&
          text_input_common::handle_undo_redo_shortcut(
              ctrl, shift, event.key, [&]() { textedit_->undo(); },
              [&]() { textedit_->redo(); },
              [&]() {
                text_input_common::finish_text_change(
                    text_, change_callback_, cursor_blink_time_, cursor_visible_, nullptr, &elem);
              })) {
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

bool InputWidget::handle_text_input(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) {
    return false;
  }
  if (textedit_) {
    textedit_->insert_text(event.text);
    text_input_common::finish_text_change(
        text_, change_callback_, cursor_blink_time_, cursor_visible_, nullptr, &elem);
    return true;
  }
  return false;
}

bool InputWidget::handle_composition_start(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) {
    return false;
  }
  (void)event;
  return text_input_common::handle_composition_start(is_composing_, composition_text_, elem);
}

bool InputWidget::handle_composition_update(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) {
    return false;
  }
  return text_input_common::handle_composition_update(
      is_composing_, composition_text_, event.composition_text, elem);
}

bool InputWidget::handle_composition_end(const Event& event, Element& elem) {
  if (readonly_ || elem.has_state("readonly")) {
    return false;
  }
  (void)event;
  return text_input_common::handle_composition_end(is_composing_, composition_text_, elem);
}

bool InputWidget::handle_mouse_down(const Event& event, Element& elem) {
  if (!textedit_) return false;

  const flex::Vec2 local_pos =
      detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
  const size_t index = x_to_index(local_pos.x, elem);
  textedit_->set_cursor(static_cast<int>(index));
  drag_anchor_ = static_cast<int>(index);
  is_dragging_ = true;

  text_input_common::reset_cursor_blink(cursor_blink_time_, cursor_visible_);

  elem.mark_paint_dirty();
  return true;
}

bool InputWidget::handle_mouse_move(const Event& event, Element& elem) {
  if (is_dragging_ && textedit_) {
    const int old_cursor = textedit_->cursor();
    const int old_selection_start = textedit_->selection_start();
    const int old_selection_end = textedit_->selection_end();
    const flex::Vec2 local_pos =
        detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
    const size_t index = x_to_index(local_pos.x, elem);
    textedit_->set_selection(drag_anchor_, static_cast<int>(index));
    if (textedit_->cursor() != old_cursor ||
        textedit_->selection_start() != old_selection_start ||
        textedit_->selection_end() != old_selection_end) {
      elem.mark_paint_dirty();
    }
    return true;
  }
  return false;
}

bool InputWidget::handle_mouse_up(const Event& event, Element& elem) {
  (void)event;
  (void)elem;
  const bool was_dragging = is_dragging_;
  is_dragging_ = false;
  return was_dragging;
}

// ============================================================================
// 坐标转换
// ============================================================================

size_t InputWidget::x_to_index(float x, const Element& elem) const {
  auto* style = elem.computed_style;
  if (!style) return 0;

  const std::string display = display_text();
  if (display.empty()) return 0;

  const auto palette = input_palette(variant_);
  const auto metrics = resolve_editable_text_metrics(
      style, effective_font_size(*this, style), Symbol("--input-text"),
      palette.text, Symbol("--input-placeholder"), palette.placeholder, 1.0f);
  float relative_x = 0.0f;
  if (is_rtl(style)) {
    relative_x = input_content_right(*this, elem, style) - x;
  } else {
    relative_x = x - effective_padding_left(*this, style);
  }
  if (relative_x <= 0.0f) return 0;
  const float letter_spacing = effective_letter_spacing(style);
  const float word_spacing = effective_word_spacing(style);
  float advance = 0.0f;
  size_t byte_pos = 0;
  bool first_glyph = true;
  while (byte_pos < display.size()) {
    const size_t start = byte_pos;
    const uint32_t cp = utf8_next_scalar(display, byte_pos).value;
    const float spacing_before = first_glyph ? 0.0f : letter_spacing;
    const float glyph_width = measure_input_codepoint_width(
        display.substr(start, byte_pos - start), cp, style, metrics);
    const float spacing_after =
        is_word_spacing_gap(cp) ? word_spacing : 0.0f;
    const float caret_before = advance;
    const float caret_after = advance + spacing_before + glyph_width + spacing_after;
    if (relative_x < caret_before + (caret_after - caret_before) * 0.5f) {
      return start;
    }
    advance = caret_after;
    first_glyph = false;
  }
  return display.size();
}

float InputWidget::index_to_x(size_t index, const Element& elem) const {
  auto* style = elem.computed_style;
  if (!style) return 0;

  const auto palette = input_palette(variant_);
  const auto metrics = resolve_editable_text_metrics(
      style, effective_font_size(*this, style), Symbol("--input-text"),
      palette.text, Symbol("--input-placeholder"), palette.placeholder, 1.0f);
  std::string display = display_text();
  const float content_right = input_content_right(*this, elem, style);
  const float text_x = input_text_origin_x(*this, elem, style, metrics, display);

  if (index == 0) return is_rtl(style) ? content_right : text_x;
  if (display.empty()) return is_rtl(style) ? content_right : text_x;

  const float letter_spacing = effective_letter_spacing(style);
  const float word_spacing = effective_word_spacing(style);
  const size_t clamped_index = std::min(index, display.size());
  float advance = 0.0f;
  size_t byte_pos = 0;

  while (byte_pos < display.size() && byte_pos < clamped_index) {
    const size_t start = byte_pos;
    const uint32_t cp = utf8_next_scalar(display, byte_pos).value;
    advance += measure_input_codepoint_width(display.substr(start, byte_pos - start), cp,
                                             style, metrics);
    if (is_word_spacing_gap(cp)) {
      advance += word_spacing;
    }
    if (byte_pos < display.size() && byte_pos < clamped_index) {
      advance += letter_spacing;
    }
  }

  return is_rtl(style) ? content_right - advance : text_x + advance;
}

std::string InputWidget::display_text() const {
  if (!password_) return text_;

  // 密码模式：返回相同长度的 *
  return std::string(utf8_scalar_count(text_), '*');
}

} // namespace flexUI
