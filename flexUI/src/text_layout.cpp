#include <flexUI/text_layout.h>
#include <flexUI/render_command.h>
#include <flexUI/text_util.h>
#include <salts_unicode.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace flexUI {

namespace {

std::string trim_copy(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string to_lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::stringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  if (lines.empty()) {
    lines.push_back("");
  }
  return lines;
}

bool is_capitalize_boundary(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' ||
         ch == '/' || ch == '.';
}

bool has_tabular_nums(const ComputedStyle* style) {
  if (!style) {
    return false;
  }
  return style->get_variable(Symbol("--font-variant-numeric"), "")
             .find("tabular-nums") != std::string::npos;
}

float base_text_width_multiplier(const ComputedStyle* style) {
  float multiplier = 0.68f;
  if (style && style->font_weight >= FontWeight::Bold) {
    multiplier = 0.85f;
  }
  if (style &&
      (style->font_family.find("Consolas") != std::string::npos ||
       style->font_family.find("Courier") != std::string::npos)) {
    multiplier = 0.65f;
  }
  return multiplier;
}

float approximate_ascii_advance(const ComputedStyle* style, char ch,
                                float font_size, float base_multiplier) {
  const unsigned char uc = static_cast<unsigned char>(ch);
  if (ch == '\t') {
    const float tab_size = style ? std::max(style->tab_size, 0.0f) : 8.0f;
    return font_size * 0.35f * tab_size;
  }
  if (std::isspace(uc)) {
    return font_size * 0.35f;
  }
  if (std::isdigit(uc)) {
    if (has_tabular_nums(style)) {
      return font_size * std::max(base_multiplier, 0.65f);
    }
    if (ch == '1') {
      return font_size * 0.42f;
    }
    if (ch == '0' || ch == '8') {
      return font_size * 0.68f;
    }
    return font_size * 0.6f;
  }
  if (std::ispunct(uc)) {
    if (ch == '.' || ch == ',' || ch == ':' || ch == ';') {
      return font_size * 0.28f;
    }
    if (ch == '-' || ch == '/' || ch == '\\') {
      return font_size * 0.36f;
    }
  }
  return font_size * base_multiplier;
}

size_t count_letter_spacing_gaps(const std::string& text) {
  size_t gaps = 0;
  size_t run_length = 0;
  for (char ch : text) {
    if (ch == '\n') {
      if (run_length > 1) {
        gaps += run_length - 1;
      }
      run_length = 0;
      continue;
    }
    ++run_length;
  }
  if (run_length > 1) {
    gaps += run_length - 1;
  }
  return gaps;
}

size_t count_word_spacing_gaps(const std::string& text) {
  size_t gaps = 0;
  for (char ch : text) {
    if (ch != '\n' && std::isspace(static_cast<unsigned char>(ch))) {
      ++gaps;
    }
  }
  return gaps;
}

float resolve_word_spacing(const ComputedStyle* style) {
  return style ? std::max(style->word_spacing, 0.0f) : 0.0f;
}

std::string trim_leading_whitespace_copy(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  return value.substr(start);
}

std::string collapse_whitespace_runs(const std::string& text) {
  std::string collapsed;
  collapsed.reserve(text.size());
  bool previous_was_space = false;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!previous_was_space) {
        collapsed.push_back(' ');
        previous_was_space = true;
      }
      continue;
    }
    collapsed.push_back(ch);
    previous_was_space = false;
  }
  return trim_copy(collapsed);
}

size_t grapheme_fit_to_width(const ComputedStyle* style,
                             const std::string& text,
                             float max_width) {
  size_t cursor = 0u;
  size_t fit = 0u;
  vstr cluster{};
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const salts_unicode_status status =
        salts_unicode_grapheme_next(input, &cursor, &cluster);
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
      throw std::invalid_argument("FlexUI text contains invalid UTF-8");
    }
    if (status != SALTS_UNICODE_OK) {
      throw std::runtime_error(
          "Salts::Unicode failed to segment grapheme clusters");
    }

    const std::string candidate = text.substr(0, cursor);
    if (approximate_text_width(style, candidate) > max_width) {
      break;
    }
    fit = cursor;
  }

  if (fit == 0u && !text.empty()) {
    cursor = 0u;
    const salts_unicode_status status =
        salts_unicode_grapheme_next(input, &cursor, &cluster);
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
      throw std::invalid_argument("FlexUI text contains invalid UTF-8");
    }
    if (status != SALTS_UNICODE_OK) {
      throw std::runtime_error(
          "Salts::Unicode failed to segment grapheme clusters");
    }
    fit = cursor;
  }

  return fit;
}

size_t last_line_break_at_or_before(const std::string& text, size_t limit) {
  size_t cursor = 0u;
  size_t break_offset = 0u;
  size_t last = std::string::npos;
  salts_unicode_line_break_opportunity opportunity =
      SALTS_UNICODE_LINE_BREAK_ALLOWED;
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const salts_unicode_status status = salts_unicode_line_break_next(
        input, &cursor, &break_offset, &opportunity);
    if (status == SALTS_UNICODE_END) {
      break;
    }
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
      throw std::invalid_argument("FlexUI text contains invalid UTF-8");
    }
    if (status != SALTS_UNICODE_OK) {
      throw std::runtime_error(
          "Salts::Unicode failed to resolve line-break opportunities");
    }
    if (break_offset > limit) {
      break;
    }
    last = break_offset;
    if (opportunity == SALTS_UNICODE_LINE_BREAK_MANDATORY) {
      break;
    }
  }

  return last;
}

std::vector<std::string> wrap_line_to_width(const ComputedStyle* style,
                                            const std::string& line,
                                            float max_width,
                                            bool preserve_spaces,
                                            bool break_word,
                                            bool break_all) {
  if (!style) {
    return {};
  }
  if (max_width <= 0.0f || line.empty() ||
      approximate_text_width(style, line) <= max_width) {
    return {line};
  }

  std::vector<std::string> wrapped;
  std::string remaining = line;

  while (!remaining.empty()) {
    if (approximate_text_width(style, remaining) <= max_width) {
      wrapped.push_back(remaining);
      break;
    }

    const size_t fit = grapheme_fit_to_width(style, remaining, max_width);
    size_t break_pos = fit;

    if (!break_all) {
      const size_t unicode_break =
          last_line_break_at_or_before(remaining, fit);
      if (unicode_break != std::string::npos) {
        break_pos = unicode_break;
      } else if (!break_word) {
        wrapped.push_back(remaining);
        break;
      }
    }

    std::string segment = remaining.substr(0, break_pos);
    remaining = remaining.substr(break_pos);

    if (!preserve_spaces) {
      segment = trim_copy(segment);
      remaining = trim_leading_whitespace_copy(remaining);
    }

    if (segment.empty()) {
      if (remaining.empty()) {
        break;
      }
      const size_t cluster_end =
          grapheme_fit_to_width(style, remaining, 0.0f);
      segment = remaining.substr(0, cluster_end);
      remaining.erase(0, cluster_end);
    }

    wrapped.push_back(segment);
  }

  if (wrapped.empty()) {
    wrapped.push_back("");
  }
  return wrapped;
}

std::string truncate_grapheme_prefix_to_width(const ComputedStyle* style,
                                              const std::string& text,
                                              float max_width,
                                              float suffix_width) {
  std::vector<size_t> grapheme_ends{0};
  size_t cursor = 0;
  vstr cluster{};
  const vstr input = vstr_from_buf(text.data(), text.size());

  while (cursor < text.size()) {
    const salts_unicode_status status =
        salts_unicode_grapheme_next(input, &cursor, &cluster);
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
      throw std::invalid_argument("FlexUI text contains invalid UTF-8");
    }
    if (status != SALTS_UNICODE_OK) {
      throw std::runtime_error("Salts::Unicode failed to segment grapheme clusters");
    }
    grapheme_ends.push_back(cursor);
  }

  std::string truncated = text;
  while (!truncated.empty() &&
         approximate_text_width(style, truncated) + suffix_width > max_width) {
    grapheme_ends.pop_back();
    truncated.resize(grapheme_ends.back());
  }
  return truncated;
}

std::string truncate_text_with_ellipsis(const ComputedStyle* style,
                                        const std::string& text,
                                        float max_width) {
  if (!style || max_width <= 0.0f) {
    return "";
  }

  if (approximate_text_width(style, text) <= max_width) {
    return text;
  }

  static const std::string ellipsis = "...";
  const float ellipsis_width = approximate_text_width(style, ellipsis);
  if (ellipsis_width > max_width) {
    return "";
  }

  return truncate_grapheme_prefix_to_width(style, text, max_width, ellipsis_width) +
         ellipsis;
}

std::string truncate_text_to_width(const ComputedStyle* style,
                                   const std::string& text,
                                   float max_width) {
  if (!style || max_width <= 0.0f) {
    return "";
  }

  if (approximate_text_width(style, text) <= max_width) {
    return text;
  }

  return truncate_grapheme_prefix_to_width(style, text, max_width, 0.0f);
}

int parse_line_clamp(const ComputedStyle* style) {
  if (!style) {
    return 0;
  }

  std::string raw = trim_copy(style->get_variable(Symbol("--max-lines"), ""));
  if (raw.empty()) {
    raw = trim_copy(style->get_variable(Symbol("--line-clamp"), ""));
  }
  if (raw.empty()) {
    return 0;
  }

  const int value = std::atoi(raw.c_str());
  return value > 0 ? value : 0;
}

float resolve_text_decoration_thickness(const ComputedStyle* style,
                                        float font_size) {
  if (!style) {
    return 1.0f;
  }

  const std::string raw = trim_copy(
      style->get_variable(Symbol("--text-decoration-thickness"), ""));
  if (raw.empty() || raw == "auto" || raw == "from-font") {
    return std::max(font_size * 0.06f, 1.0f);
  }
  if (!raw.empty() && raw.back() == '%') {
    return std::max(
        font_size * std::strtof(raw.substr(0, raw.size() - 1).c_str(), nullptr) /
            100.0f,
        1.0f);
  }

  const float value = std::strtof(raw.c_str(), nullptr);
  return value > 0.0f ? value : std::max(font_size * 0.06f, 1.0f);
}

float resolve_text_underline_offset(const ComputedStyle* style, float font_size) {
  if (!style) {
    return 0.0f;
  }

  const std::string raw =
      trim_copy(style->get_variable(Symbol("--text-underline-offset"), ""));
  if (raw.empty() || raw == "auto") {
    return font_size * 0.1f;
  }
  if (!raw.empty() && raw.back() == '%') {
    return font_size * std::strtof(raw.substr(0, raw.size() - 1).c_str(), nullptr) /
           100.0f;
  }

  return std::strtof(raw.c_str(), nullptr);
}

BorderStyle resolve_text_decoration_style(const ComputedStyle* style) {
  if (!style) {
    return BorderStyle::Solid;
  }
  const std::string value =
      to_lower_copy(trim_copy(style->get_variable(
          Symbol("--text-decoration-style"), "solid")));
  if (value == "dashed") {
    return BorderStyle::Dashed;
  }
  if (value == "dotted") {
    return BorderStyle::Dotted;
  }
  if (value == "double") {
    return BorderStyle::Double;
  }
  if (value == "wavy") {
    return BorderStyle::Wavy;
  }
  if (value == "none") {
    return BorderStyle::None;
  }
  return BorderStyle::Solid;
}

float resolve_text_line_through_offset(float font_size) {
  return -font_size * 0.3f;
}

std::vector<std::string> normalize_text_lines(const ComputedStyle* style,
                                              const std::string& text,
                                              float max_width) {
  std::vector<std::string> lines;
  if (!style) {
    return lines;
  }

  const std::string transformed = transform_text_for_layout(style, text);

  const std::string white_space =
      trim_copy(style->get_variable(Symbol("--white-space"), ""));
  const std::string text_wrap =
      trim_copy(style->get_variable(Symbol("--text-wrap"), ""));
  const std::string text_wrap_mode =
      trim_copy(style->get_variable(Symbol("--text-wrap-mode"), ""));
  const std::string overflow_wrap =
      trim_copy(style->get_variable(Symbol("--overflow-wrap"), ""));
  const std::string word_break =
      trim_copy(style->get_variable(Symbol("--word-break"), ""));
  const bool preserve_spaces = white_space == "pre" ||
                               white_space == "pre-wrap" ||
                               white_space == "break-spaces";
  const bool no_wrap = white_space == "pre" || white_space == "nowrap" ||
                       text_wrap == "nowrap" ||
                       text_wrap_mode == "nowrap";
  const bool collapse_spaces =
      white_space.empty() || white_space == "normal" || white_space == "nowrap" ||
      white_space == "pre-line";
  const bool break_word =
      overflow_wrap == "break-word" || overflow_wrap == "anywhere" ||
      word_break == "break-word";
  const bool break_all =
      word_break == "break-all" || overflow_wrap == "anywhere";

  std::vector<std::string> base_lines;
  if (white_space == "nowrap") {
    std::string single_line = transformed;
    std::replace(single_line.begin(), single_line.end(), '\n', ' ');
    if (collapse_spaces) {
      single_line = collapse_whitespace_runs(single_line);
    }
    base_lines.push_back(single_line);
  } else {
    base_lines = split_lines(transformed);
    if (collapse_spaces) {
      for (auto& line : base_lines) {
        line = collapse_whitespace_runs(line);
      }
    }
  }

  for (const auto& base_line : base_lines) {
    if (no_wrap) {
      lines.push_back(base_line);
    } else {
      const auto wrapped = wrap_line_to_width(style, base_line, max_width,
                                              preserve_spaces, break_word,
                                              break_all);
      lines.insert(lines.end(), wrapped.begin(), wrapped.end());
    }
  }
  if (lines.empty()) {
    lines.push_back("");
  }

  const int line_clamp = parse_line_clamp(style);
  bool clamped = false;
  if (line_clamp > 0 && static_cast<int>(lines.size()) > line_clamp) {
    lines.resize(static_cast<size_t>(line_clamp));
    clamped = true;
  }

  const std::string text_overflow =
      trim_copy(style->get_variable(Symbol("--text-overflow"), ""));
  if (!lines.empty() && white_space == "nowrap") {
    if (text_overflow == "ellipsis") {
      lines.front() = truncate_text_with_ellipsis(style, lines.front(), max_width);
    } else if (text_overflow == "clip") {
      lines.front() = truncate_text_to_width(style, lines.front(), max_width);
    }
  }

  if (clamped && !lines.empty()) {
    lines.back() = truncate_text_with_ellipsis(style, lines.back() + "...", max_width);
    if (lines.back().empty()) {
      lines.back() = "...";
    }
  }

  return lines;
}

TextAlign parse_text_align_keyword(const std::string& raw, TextAlign fallback) {
  const std::string value = to_lower_copy(trim_copy(raw));
  if (value == "left") {
    return TextAlign::Left;
  }
  if (value == "center") {
    return TextAlign::Center;
  }
  if (value == "right") {
    return TextAlign::Right;
  }
  if (value == "justify") {
    return TextAlign::Justify;
  }
  if (value == "start") {
    return TextAlign::Start;
  }
  if (value == "end") {
    return TextAlign::End;
  }
  return fallback;
}

TextAlign resolve_text_align_last(const ComputedStyle* style,
                                  TextAlign base_align) {
  if (!style) {
    return base_align;
  }
  const std::string raw =
      style->get_variable(Symbol("--text-align-last"), "");
  const std::string value = to_lower_copy(trim_copy(raw));
  if (value.empty()) {
    return base_align;
  }
  if (value == "auto") {
    return base_align == TextAlign::Justify ? TextAlign::Start : base_align;
  }
  return parse_text_align_keyword(value, base_align);
}

} // namespace

Direction resolve_text_direction_for_content(const ComputedStyle* style,
                                             const std::string& text) {
  const Direction fallback = style ? style->direction : Direction::Ltr;
  if (!style || style->unicode_bidi != UnicodeBidi::Plaintext || text.empty()) {
    return fallback;
  }

  uint8_t paragraph_level = 0u;
  const salts_unicode_status status = salts_unicode_bidi_paragraph_level(
      vstr_from_buf(text.data(), text.size()), &paragraph_level);
  if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
    throw std::invalid_argument("FlexUI text contains invalid UTF-8");
  }
  if (status != SALTS_UNICODE_OK) {
    throw std::runtime_error("Salts::Unicode failed to resolve paragraph direction");
  }
  return paragraph_level == 0u ? Direction::Ltr : Direction::Rtl;
}

std::string transform_text_for_layout(const ComputedStyle* style,
                                      const std::string& text) {
  if (!style || text.empty()) {
    return text;
  }

  std::string transformed = text;
  switch (style->text_transform) {
  case TextTransform::Uppercase:
    std::transform(
        transformed.begin(), transformed.end(), transformed.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return transformed;
  case TextTransform::Lowercase:
    std::transform(
        transformed.begin(), transformed.end(), transformed.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return transformed;
  case TextTransform::Capitalize: {
    bool capitalize_next = true;
    for (char& ch : transformed) {
      if (capitalize_next &&
          std::isalpha(static_cast<unsigned char>(ch))) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        capitalize_next = false;
        continue;
      }
      capitalize_next = is_capitalize_boundary(ch);
    }
    return transformed;
  }
  case TextTransform::None:
  default:
    return transformed;
  }
}

float approximate_text_width(const ComputedStyle* style, const std::string& text) {
  if (!style) {
    return 0.0f;
  }

  const std::string transformed = transform_text_for_layout(style, text);
  const float font_size = style->font_size > 0.0f ? style->font_size : 14.0f;
  const float multiplier = base_text_width_multiplier(style);
  float base_width = 0.0f;
  for (char ch : transformed) {
    if (ch == '\n') {
      continue;
    }
    base_width +=
        approximate_ascii_advance(style, ch, font_size, multiplier);
  }
  const float spacing_width =
      static_cast<float>(count_letter_spacing_gaps(transformed)) *
      std::max(style->letter_spacing, 0.0f);
  const float word_spacing_width =
      static_cast<float>(count_word_spacing_gaps(transformed)) *
      resolve_word_spacing(style);
  return base_width + spacing_width + word_spacing_width;
}

float approximate_segmented_text_width(const ComputedStyle* style,
                                       const std::string& text) {
  if (!style) {
    return 0.0f;
  }

  const std::string transformed = transform_text_for_layout(style, text);
  ComputedStyle measure_style = *style;
  measure_style.letter_spacing = 0.0f;
  measure_style.word_spacing = 0.0f;

  float width = 0.0f;
  size_t codepoint_count = 0;
  for (const auto& segment : segment_text(transformed)) {
    const size_t segment_codepoints = utf8_scalar_count(segment.text);
    if (segment.type == TextSegmentType::Emoji) {
      width += static_cast<float>(segment_codepoints) * measure_style.font_size;
    } else {
      width += approximate_text_width(&measure_style, segment.text);
    }
    codepoint_count += segment_codepoints;
  }

  if (codepoint_count > 1) {
    width += std::max(style->letter_spacing, 0.0f) *
             static_cast<float>(codepoint_count - 1);
  }
  width += static_cast<float>(count_word_spacing_gaps(transformed)) *
           resolve_word_spacing(style);
  return width;
}

float emit_segmented_text_line(RenderCommandList& commands, const ComputedStyle* style,
                               const std::string& text, float x, float baseline_y,
                               const Color& color, bool bold) {
  if (!style || text.empty()) {
    return 0.0f;
  }

  const std::string transformed = transform_text_for_layout(style, text);
  ComputedStyle measure_style = *style;
  measure_style.letter_spacing = 0.0f;
  measure_style.word_spacing = 0.0f;

  const auto emit_segments = [&](float origin_x, float origin_y,
                                 const Color& draw_color) {
    float current_x = origin_x;
    const float letter_spacing = std::max(style->letter_spacing, 0.0f);
    const float word_spacing = resolve_word_spacing(style);
    bool has_previous_codepoint = false;
    for (const auto& segment : segment_text(transformed)) {
      const size_t segment_codepoints = utf8_scalar_count(segment.text);
      if (segment_codepoints == 0) {
        continue;
      }

      if (has_previous_codepoint) {
        current_x += letter_spacing;
      }

      if (segment.type == TextSegmentType::Emoji) {
        commands.draw_text(segment.text, current_x, origin_y,
                           get_emoji_font_name(), style->font_size, bold,
                           draw_color);
        current_x += static_cast<float>(segment_codepoints) * style->font_size;
      } else {
        const std::string font_name =
            style->font_family.empty() ? "Arial" : style->font_family;
        const bool has_tab = segment.text.find('\t') != std::string::npos;
        if (word_spacing <= 0.0f && !has_tab) {
          commands.draw_text(segment.text, current_x, origin_y, font_name,
                             style->font_size, bold, draw_color);
          current_x += approximate_text_width(&measure_style, segment.text);
        } else {
        size_t run_start = 0;
        for (size_t i = 0; i <= segment.text.size(); ++i) {
          const bool at_end = i == segment.text.size();
          const bool at_space =
              !at_end &&
              std::isspace(static_cast<unsigned char>(segment.text[i]));
          if (!at_end && !at_space) {
            continue;
          }
          if (i > run_start) {
            const std::string run = segment.text.substr(run_start, i - run_start);
            commands.draw_text(run, current_x, origin_y, font_name,
                               style->font_size, bold, draw_color);
            current_x += approximate_text_width(&measure_style, run);
          }
          if (at_space) {
            current_x += approximate_text_width(
                             &measure_style,
                             std::string(1, segment.text[i])) +
                         word_spacing;
            run_start = i + 1;
          }
        }
        }
      }

      if (segment_codepoints > 1) {
        current_x += letter_spacing * static_cast<float>(segment_codepoints - 1);
      }
      has_previous_codepoint = true;
    }
    return current_x - origin_x;
  };

  if (style->has_text_shadow) {
    for (const auto& shadow : style->text_shadows) {
      if (shadow.color.a <= 0.0f) {
        continue;
      }
      const bool use_blur = shadow.blur_radius > 0.0f;
      if (use_blur) {
        commands.save();
        commands.set_blur(flex::BlurFilter(shadow.blur_radius));
      }
      emit_segments(x + shadow.offset_x, baseline_y + shadow.offset_y,
                    shadow.color);
      if (use_blur) {
        commands.restore();
      }
    }
  }

  return emit_segments(x, baseline_y, color);
}

float resolve_line_height(const ComputedStyle* style) {
  if (!style) {
    return 0.0f;
  }

  const float font_size = style->font_size > 0.0f ? style->font_size : 16.0f;
  const std::string raw = trim_copy(style->get_variable(Symbol("--line-height"), ""));
  if (raw.empty() || raw == "normal") {
    return font_size * 1.6f;
  }
  if (!raw.empty() && raw.back() == '%') {
    return font_size * std::strtof(raw.substr(0, raw.size() - 1).c_str(), nullptr) /
           100.0f;
  }
  if (raw.size() >= 2 && raw.compare(raw.size() - 2, 2, "px") == 0) {
    return std::strtof(raw.substr(0, raw.size() - 2).c_str(), nullptr);
  }
  if (raw.size() >= 2 && raw.compare(raw.size() - 2, 2, "em") == 0) {
    return font_size * std::strtof(raw.substr(0, raw.size() - 2).c_str(), nullptr);
  }
  if (raw.size() >= 3 && raw.compare(raw.size() - 3, 3, "rem") == 0) {
    return 16.0f * std::strtof(raw.substr(0, raw.size() - 3).c_str(), nullptr);
  }

  const float numeric = std::strtof(raw.c_str(), nullptr);
  return numeric > 0.0f ? font_size * numeric : font_size * 1.6f;
}

float resolve_line_height_with_default_multiplier(const ComputedStyle* style,
                                                  float default_multiplier) {
  if (!style) {
    return 0.0f;
  }

  const std::string raw = trim_copy(style->get_variable(Symbol("--line-height"), ""));
  if (raw.empty() || raw == "normal") {
    const float font_size = style->font_size > 0.0f ? style->font_size : 16.0f;
    return font_size * default_multiplier;
  }
  return resolve_line_height(style);
}

float resolve_line_text_top(float line_top, const EditableTextMetrics& metrics) {
  return line_top + std::max(metrics.line_height - metrics.font_size, 0.0f) * 0.5f;
}

TextVerticalAlign resolve_text_vertical_align(const ComputedStyle* style,
                                              TextVerticalAlign fallback) {
  if (!style) {
    return fallback;
  }

  const std::string raw =
      trim_copy(style->get_variable(Symbol("--vertical-align"), ""));
  if (raw == "top") {
    return TextVerticalAlign::Top;
  }
  if (raw == "bottom") {
    return TextVerticalAlign::Bottom;
  }
  if (raw == "middle" || raw == "center") {
    return TextVerticalAlign::Middle;
  }
  return fallback;
}

EditableTextMetrics resolve_editable_text_metrics(
    const ComputedStyle* style, float font_size_fallback,
    Symbol text_color_var, const Color& default_text_color,
    Symbol placeholder_color_var, const Color& default_placeholder_color,
    float default_line_height_multiplier) {
  EditableTextMetrics metrics;
  if (!style) {
    return metrics;
  }

  metrics.font_family = style->font_family.empty() ? "Arial" : style->font_family;
  metrics.font_size = font_size_fallback > 0.0f ? font_size_fallback : style->font_size;
  if (metrics.font_size <= 0.0f) {
    metrics.font_size = 16.0f;
  }
  metrics.line_height =
      resolve_line_height_with_default_multiplier(style, default_line_height_multiplier);
  metrics.char_width =
      approximate_text_width(style, has_tabular_nums(style) ? "0" : "a");
  if (metrics.char_width <= 0.0f) {
    metrics.char_width = metrics.font_size * 0.6f;
  }
  metrics.padding_left = style->padding[3];
  metrics.padding_right = style->padding[1];
  metrics.padding_top = style->padding[0];
  metrics.direction = style->direction;
  metrics.text_color = style->get_variable_color(
      text_color_var, style->get_variable_color(Symbol("--text-color"), default_text_color));
  metrics.placeholder_color =
      style->get_variable_color(placeholder_color_var, default_placeholder_color);
  return metrics;
}

TextLayoutBlock layout_text_block(const ComputedStyle* style, const std::string& text,
                                  float x, float y, float width, float height,
                                  const Color& color,
                                  TextVerticalAlign vertical_align) {
  TextLayoutBlock block;
  if (!style || text.empty() || width <= 0.0f || height <= 0.0f) {
    return block;
  }

  block.color = color;
  block.decoration_color = style->get_variable_color(
      Symbol("--text-decoration-color"), block.color);
  if (style->has_text_shadow) {
    block.text_shadows = style->text_shadows;
  }
  block.font_family = style->font_family.empty() ? "Arial" : style->font_family;
  block.font_size = style->font_size;
  block.bold = style->font_weight >= FontWeight::Bold;
  block.tabular_nums = has_tabular_nums(style);
  block.letter_spacing = std::max(style->letter_spacing, 0.0f);
  block.word_spacing = resolve_word_spacing(style);
  block.text_indent = style->text_indent;
  block.tab_size = style->tab_size;
  block.line_height = resolve_line_height(style);
  block.decoration_thickness =
      resolve_text_decoration_thickness(style, block.font_size);
  block.decoration_style = resolve_text_decoration_style(style);
  block.underline_offset =
      resolve_text_underline_offset(style, block.font_size);
  block.line_through_offset = resolve_text_line_through_offset(block.font_size);

  const std::string text_decoration =
      trim_copy(style->get_variable(Symbol("--text-decoration"), ""));
  block.overline = text_decoration.find("overline") != std::string::npos;
  block.underline = text_decoration.find("underline") != std::string::npos;
  block.line_through =
      text_decoration.find("line-through") != std::string::npos;

  const auto lines = normalize_text_lines(style, text, width);
  if (lines.empty()) {
    return block;
  }

  const float total_height = block.line_height * static_cast<float>(lines.size());
  float top_offset = 0.0f;
  if (vertical_align == TextVerticalAlign::Middle) {
    top_offset = std::max(height - total_height, 0.0f) * 0.5f;
  } else if (vertical_align == TextVerticalAlign::Bottom) {
    top_offset = std::max(height - total_height, 0.0f);
  }

  block.lines.reserve(lines.size());
  for (size_t i = 0; i < lines.size(); ++i) {
    const float line_width = approximate_text_width(style, lines[i]);
    const bool is_last_line = i + 1 == lines.size();
    const std::string text_align_last_value =
        is_last_line
            ? to_lower_copy(trim_copy(
                  style->get_variable(Symbol("--text-align-last"), "")))
            : "";
    TextAlign align = style->text_align;
    if (is_last_line && !text_align_last_value.empty()) {
      align = resolve_text_align_last(style, style->text_align);
    }
    const Direction line_direction =
        resolve_text_direction_for_content(style, lines[i]);
    const float indent =
        i == 0 ? (line_direction == Direction::Rtl ? -block.text_indent
                                                   : block.text_indent)
               : 0.0f;
    float line_x = x + indent;
    if (align == TextAlign::Start) {
      align = line_direction == Direction::Rtl ? TextAlign::Right
                                               : TextAlign::Left;
    } else if (align == TextAlign::End) {
      align = line_direction == Direction::Rtl ? TextAlign::Left
                                               : TextAlign::Right;
    }
    if (align == TextAlign::Center) {
      line_x += std::max(width - line_width, 0.0f) * 0.5f;
    } else if (align == TextAlign::Right) {
      line_x += std::max(width - line_width, 0.0f);
    }
    float justify_spacing = 0.0f;
    float visual_line_width = line_width;
    const bool justify_line =
        align == TextAlign::Justify &&
        (!is_last_line || text_align_last_value == "justify");
    if (justify_line) {
      const size_t gaps = count_word_spacing_gaps(lines[i]);
      if (gaps > 0 && line_width < width) {
        justify_spacing = (width - line_width) / static_cast<float>(gaps);
        visual_line_width = width;
      }
    }

    const float line_y =
        y + top_offset + static_cast<float>(i) * block.line_height +
        std::max(block.line_height - block.font_size, 0.0f) * 0.5f;
    block.lines.push_back(
        {lines[i], line_x, line_y, visual_line_width, justify_spacing});
  }

  return block;
}

void emit_text_decoration_line(RenderCommandList& commands, float x1, float y,
                               float x2, const Color& color, float thickness,
                               BorderStyle style) {
  if (thickness <= 0.0f || color.a <= 0.0f || style == BorderStyle::None ||
      x2 <= x1) {
    return;
  }
  if (style == BorderStyle::Solid) {
    commands.draw_line(x1, y, x2, y, Paint::solid(color), thickness);
    return;
  }
  if (style == BorderStyle::Double) {
    const float offset = std::max(thickness * 1.5f, 1.0f);
    commands.draw_line(x1, y - offset, x2, y - offset, Paint::solid(color),
                       thickness);
    commands.draw_line(x1, y + offset, x2, y + offset, Paint::solid(color),
                       thickness);
    return;
  }
  if (style == BorderStyle::Wavy) {
    const float amplitude = std::max(thickness * 1.25f, 1.0f);
    const float step = std::max(thickness * 2.0f, 2.0f);
    float cursor = x1;
    float current_y = y;
    bool up = true;
    while (cursor < x2) {
      const float next_x = std::min(x2, cursor + step);
      const float next_y = y + (up ? -amplitude : amplitude);
      commands.draw_line(cursor, current_y, next_x, next_y,
                         Paint::solid(color), thickness);
      cursor = next_x;
      current_y = next_y;
      up = !up;
    }
    return;
  }

  const float length = x2 - x1;
  const float dash_length =
      style == BorderStyle::Dashed ? std::max(thickness * 3.0f, thickness)
                                   : std::max(thickness, 1.0f);
  const float gap_length =
      style == BorderStyle::Dashed ? std::max(thickness * 2.0f, thickness)
                                   : std::max(thickness * 1.5f, thickness);
  for (float cursor = 0.0f; cursor < length;
       cursor += dash_length + gap_length) {
    const float seg_start = cursor;
    const float seg_end = std::min(length, cursor + dash_length);
    commands.draw_line(x1 + seg_start, y, x1 + seg_end, y,
                       Paint::solid(color), thickness);
  }
}

void emit_text_block(RenderCommandList& commands, const TextLayoutBlock& block) {
  const auto emit_line_segments = [&](const TextLayoutLine& line,
                                      const Color& color, float dx,
                                      float dy) {
    float current_x = line.x + dx;
    const auto segments = segment_text(line.text);
    for (size_t i = 0; i < segments.size(); ++i) {
      const auto& segment = segments[i];
      const std::string font_name =
          segment.type == TextSegmentType::Emoji ? get_emoji_font_name()
                                                 : block.font_family;

      if (segment.type == TextSegmentType::Emoji) {
        commands.draw_text(segment.text, current_x, line.baseline_y + dy,
                           font_name, block.font_size, block.bold, color);
        size_t count = 0;
        size_t pos = 0;
        while (pos < segment.text.size()) {
          utf8_next_scalar(segment.text, pos).value;
          count++;
        }
        current_x += static_cast<float>(count) * block.font_size;
      } else {
        ComputedStyle measure_style;
        measure_style.font_family = block.font_family;
        measure_style.font_size = block.font_size;
        measure_style.font_weight =
            block.bold ? FontWeight::Bold : FontWeight::Normal;
        measure_style.letter_spacing = block.letter_spacing;
        measure_style.word_spacing = 0.0f;
        measure_style.tab_size = block.tab_size;
        if (block.tabular_nums) {
          measure_style.variables[Symbol("--font-variant-numeric")] =
              "tabular-nums";
        }
        const float effective_word_spacing =
            block.word_spacing + line.justify_spacing;
        const bool has_tab = segment.text.find('\t') != std::string::npos;
        if (effective_word_spacing <= 0.0f && !has_tab) {
          commands.draw_text(segment.text, current_x, line.baseline_y + dy,
                             font_name, block.font_size, block.bold, color);
          current_x += approximate_text_width(&measure_style, segment.text);
        } else {
          size_t run_start = 0;
          for (size_t j = 0; j <= segment.text.size(); ++j) {
            const bool at_end = j == segment.text.size();
            const bool at_space =
                !at_end &&
                std::isspace(static_cast<unsigned char>(segment.text[j]));
            if (!at_end && !at_space) {
              continue;
            }
            if (j > run_start) {
              const std::string run =
                  segment.text.substr(run_start, j - run_start);
              commands.draw_text(run, current_x, line.baseline_y + dy,
                                 font_name, block.font_size, block.bold, color);
              current_x += approximate_text_width(&measure_style, run);
            }
            if (at_space) {
              current_x += approximate_text_width(
                               &measure_style,
                               std::string(1, segment.text[j])) +
                           effective_word_spacing;
              run_start = j + 1;
            }
          }
        }
      }
      if (i + 1 < segments.size()) {
        current_x += block.letter_spacing;
      }
    }
  };

  for (const auto& line : block.lines) {
    if (line.text.empty()) {
      continue;
    }

    for (const auto& shadow : block.text_shadows) {
      if (shadow.color.a <= 0.0f) {
        continue;
      }
      const bool use_blur = shadow.blur_radius > 0.0f;
      if (use_blur) {
        commands.save();
        commands.set_blur(flex::BlurFilter(shadow.blur_radius));
      }
      emit_line_segments(line, shadow.color, shadow.offset_x,
                         shadow.offset_y);
      if (use_blur) {
        commands.restore();
      }
    }
    emit_line_segments(line, block.color, 0.0f, 0.0f);

    if (block.overline && line.width > 0.0f) {
      emit_text_decoration_line(commands, line.x, line.baseline_y,
                                line.x + line.width, block.decoration_color,
                                block.decoration_thickness,
                                block.decoration_style);
    }
    if (block.underline && line.width > 0.0f) {
      const float underline_y =
          line.baseline_y + block.font_size + block.underline_offset;
      emit_text_decoration_line(commands, line.x, underline_y,
                                line.x + line.width, block.decoration_color,
                                block.decoration_thickness,
                                block.decoration_style);
    }
    if (block.line_through && line.width > 0.0f) {
      const float strike_y =
          line.baseline_y + block.font_size * 0.5f + block.line_through_offset;
      emit_text_decoration_line(commands, line.x, strike_y,
                                line.x + line.width, block.decoration_color,
                                block.decoration_thickness,
                                block.decoration_style);
    }
  }
}

} // namespace flexUI
