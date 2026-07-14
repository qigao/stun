/*
 * flexUI - LayoutManager Implementation
 */

#include <flexUI/layout_manager.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_length.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace flexUI {

namespace {

detail::CssLengthContext make_length_context(const Element* elem,
                                             float container_size) {
  detail::CssLengthContext context;
  context.percent_reference = container_size;
  if (elem && elem->owner_box_) {
    context.viewport_width = elem->owner_box_->viewport_width();
    context.viewport_height = elem->owner_box_->viewport_height();
  }
  return context;
}

float parse_constraint_length(const std::string& value, float container_size,
                              const Element* elem = nullptr) {
  return detail::parse_css_length(value, make_length_context(elem, container_size));
}

float clamp_dimension(float value,
                      const std::optional<float>& min_value,
                      const std::optional<float>& max_value) {
  if (min_value.has_value()) {
    value = std::max(value, *min_value);
  }

  if (max_value.has_value()) {
    const float upper = min_value.has_value() ? std::max(*max_value, *min_value)
                                              : *max_value;
    value = std::min(value, upper);
  }

  return value;
}

bool is_scroll_overflow(Overflow value) {
  return value == Overflow::Auto || value == Overflow::Scroll;
}

std::pair<float, float> parent_content_size(Element* elem) {
  auto* parent = elem ? elem->parent_elem() : nullptr;
  if (!parent) {
    return {elem ? elem->layout_width() : 0.0f,
            elem ? elem->layout_height() : 0.0f};
  }

  float width = parent->layout_width();
  float height = parent->layout_height();
  if (auto* style = parent->computed_style) {
    width = std::max(0.0f, width - style->padding[1] - style->padding[3] -
                               style->border_width[1] - style->border_width[3]);
    height = std::max(0.0f, height - style->padding[0] - style->padding[2] -
                                style->border_width[0] - style->border_width[2]);
  }
  return {width, height};
}

void apply_layout_constraints(Element* elem) {
  if (!elem || !elem->computed_style) {
    return;
  }

  auto* style = elem->computed_style;
  const auto [container_w, container_h] = parent_content_size(elem);
  const float horizontal_extras = style->padding[1] + style->padding[3] +
                                  style->border_width[1] + style->border_width[3];
  const float vertical_extras = style->padding[0] + style->padding[2] +
                                style->border_width[0] + style->border_width[2];

  auto outer_width_for_spec = [&](float specified) {
    return style->box_sizing == BoxSizing::BorderBox ? specified
                                                     : specified + horizontal_extras;
  };
  auto outer_height_for_spec = [&](float specified) {
    return style->box_sizing == BoxSizing::BorderBox ? specified
                                                     : specified + vertical_extras;
  };
  auto parse_outer_constraint = [&](Symbol name, float container_size,
                                    auto&& outer_for_spec) -> std::optional<float> {
    const std::string raw = style->get_variable(name, "");
    if (raw.empty() || raw == "auto" || raw == "none") {
      return std::nullopt;
    }

    const float specified = parse_constraint_length(raw, container_size, elem);
    if (std::isnan(specified)) {
      return std::nullopt;
    }
    return outer_for_spec(specified);
  };

  const auto min_width =
      parse_outer_constraint(Symbol("min-width"), container_w, outer_width_for_spec);
  const auto max_width =
      parse_outer_constraint(Symbol("max-width"), container_w, outer_width_for_spec);
  const auto min_height =
      parse_outer_constraint(Symbol("min-height"), container_h, outer_height_for_spec);
  const auto max_height =
      parse_outer_constraint(Symbol("max-height"), container_h, outer_height_for_spec);

  if (min_width.has_value() || max_width.has_value()) {
    elem->set_layout_width(
        clamp_dimension(elem->layout_width(), min_width, max_width));
  }
  if (min_height.has_value() || max_height.has_value()) {
    elem->set_layout_height(
        clamp_dimension(elem->layout_height(), min_height, max_height));
  }
}

void update_scroll_metrics(Element* elem) {
  if (!elem || !elem->computed_style) {
    return;
  }

  auto* style = elem->computed_style;
  const bool scroll_x = is_scroll_overflow(style->overflow_x);
  const bool scroll_y = is_scroll_overflow(style->overflow_y);
  if (!scroll_x && !scroll_y) {
    elem->set_scroll_metrics(false, 0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  float content_width = elem->layout_width();
  float content_height = elem->layout_height();
  for (auto* node : elem->children()) {
    auto* child = static_cast<Element*>(node);
    if (!child || !child->is_visible()) {
      continue;
    }
    if (child->computed_style &&
        child->computed_style->position == Position::Fixed) {
      continue;
    }

    float right = child->x() + child->layout_width();
    float bottom = child->y() + child->layout_height();
    if (child->computed_style) {
      right += child->computed_style->margin[1];
      bottom += child->computed_style->margin[2];
    }
    content_width = std::max(content_width, right);
    content_height = std::max(content_height, bottom);
  }

  elem->set_scroll_metrics(true, content_width, content_height,
                           std::max(content_width - elem->layout_width(), 0.0f),
                           std::max(content_height - elem->layout_height(), 0.0f));
}

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

float parse_aspect_ratio_value(const std::string& raw) {
  const std::string value = trim_copy(raw);
  if (value.empty() || value == "auto") {
    return NAN;
  }

  const size_t slash = value.find('/');
  if (slash == std::string::npos) {
    const float ratio = std::strtof(value.c_str(), nullptr);
    return ratio > 0.0f ? ratio : NAN;
  }

  const float numerator =
      std::strtof(trim_copy(value.substr(0, slash)).c_str(), nullptr);
  const float denominator =
      std::strtof(trim_copy(value.substr(slash + 1)).c_str(), nullptr);
  if (numerator <= 0.0f || denominator <= 0.0f) {
    return NAN;
  }
  return numerator / denominator;
}

float parse_line_height_value(const ComputedStyle* style) {
  if (!style) {
    return NAN;
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

float measure_text_leaf_height(const ComputedStyle* style, const std::string& text,
                               float available_width) {
  if (!style || text.empty()) {
    return 0.0f;
  }

  float line_height = resolve_line_height(style);
  if (!(line_height > 0.0f)) {
    line_height = parse_line_height_value(style);
  }
  if (!(line_height > 0.0f)) {
    line_height = style->font_size > 0.0f ? style->font_size * 1.6f : 24.0f;
  }

  const float layout_width = std::max(available_width, style->font_size > 0.0f
                                                           ? style->font_size
                                                           : 1.0f);
  const auto block =
      layout_text_block(style, text, 0.0f, 0.0f, layout_width, line_height,
                        Color{0.0f, 0.0f, 0.0f, 1.0f}, TextVerticalAlign::Top);
  const size_t line_count = std::max<size_t>(block.lines.size(), 1);
  return line_height * static_cast<float>(line_count);
}

float measure_wrapped_flex_cross_size(Element* elem, bool is_row, float gap) {
  if (!elem) {
    return 0.0f;
  }

  const float available_main =
      std::max(is_row ? elem->layout_width() : elem->layout_height(), 0.0f);
  if (!(available_main > 0.0f)) {
    return 0.0f;
  }

  float total_cross = 0.0f;
  float line_main = 0.0f;
  float line_cross = 0.0f;
  int line_count = 0;

  auto push_line = [&]() {
    if (line_cross <= 0.0f) {
      return;
    }
    total_cross += line_cross;
    ++line_count;
    line_main = 0.0f;
    line_cross = 0.0f;
  };

  for (auto* node : elem->children()) {
    auto* child = static_cast<Element*>(node);
    if (!child || !child->is_visible()) {
      continue;
    }

    auto* cs = child->computed_style;
    if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
      continue;
    }

    const float child_main = is_row ? child->layout_width() : child->layout_height();
    const float child_cross = is_row ? child->layout_height() : child->layout_width();
    const float margin_main_start = cs ? (is_row ? cs->margin[3] : cs->margin[0]) : 0.0f;
    const float margin_main_end = cs ? (is_row ? cs->margin[1] : cs->margin[2]) : 0.0f;
    const float margin_cross_start = cs ? (is_row ? cs->margin[0] : cs->margin[3]) : 0.0f;
    const float margin_cross_end = cs ? (is_row ? cs->margin[2] : cs->margin[1]) : 0.0f;

    const float item_main = child_main + margin_main_start + margin_main_end;
    const float item_cross = child_cross + margin_cross_start + margin_cross_end;
    const float projected_main = line_main <= 0.0f ? item_main : line_main + gap + item_main;

    if (line_main > 0.0f && projected_main > available_main + 0.001f) {
      push_line();
    }

    line_main = line_main <= 0.0f ? item_main : line_main + gap + item_main;
    line_cross = std::max(line_cross, item_cross);
  }

  push_line();
  if (line_count > 1) {
    total_cross += gap * static_cast<float>(line_count - 1);
  }
  return total_cross;
}

std::vector<std::string> split_grid_track_tokens(const std::string& value) {
  std::vector<std::string> tokens;
  std::string current;
  int depth = 0;
  for (char ch : value) {
    if (ch == '(') {
      ++depth;
      current.push_back(ch);
      continue;
    }
    if (ch == ')') {
      depth = std::max(depth - 1, 0);
      current.push_back(ch);
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch)) && depth == 0) {
      const std::string token = trim_copy(current);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }

  const std::string tail = trim_copy(current);
  if (!tail.empty()) {
    tokens.push_back(tail);
  }
  return tokens;
}

struct GridTrackSpec {
  float base_size = 0.0f;
  float fr_units = 0.0f;
};

GridTrackSpec parse_grid_track_spec(const std::string& token,
                                    float container_size,
                                    const Element* elem = nullptr) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.empty() || trimmed == "auto") {
    return {};
  }

  if (trimmed.rfind("minmax(", 0) == 0 && trimmed.back() == ')') {
    const std::string inner = trimmed.substr(7, trimmed.size() - 8);
    int depth = 0;
    size_t comma_pos = std::string::npos;
    for (size_t i = 0; i < inner.size(); ++i) {
      if (inner[i] == '(') {
        ++depth;
      } else if (inner[i] == ')') {
        depth = std::max(depth - 1, 0);
      } else if (inner[i] == ',' && depth == 0) {
        comma_pos = i;
        break;
      }
    }

    if (comma_pos != std::string::npos) {
      const std::string min_text = trim_copy(inner.substr(0, comma_pos));
      const std::string max_text = trim_copy(inner.substr(comma_pos + 1));

      GridTrackSpec spec;
      const float min_size = parse_constraint_length(min_text, container_size, elem);
      if (!std::isnan(min_size) && min_size > 0.0f) {
        spec.base_size = min_size;
      }

      if (max_text.size() >= 2 &&
          max_text.compare(max_text.size() - 2, 2, "fr") == 0) {
        spec.fr_units = std::max(
            std::strtof(max_text.substr(0, max_text.size() - 2).c_str(), nullptr),
            0.0f);
        if (spec.fr_units == 0.0f) {
          spec.fr_units = 1.0f;
        }
        return spec;
      }

      const float max_size = parse_constraint_length(max_text, container_size, elem);
      if (!std::isnan(max_size) && max_size > spec.base_size) {
        spec.base_size = max_size;
      }
      return spec;
    }
  }

  if (trimmed.size() >= 2 &&
      trimmed.compare(trimmed.size() - 2, 2, "fr") == 0) {
    GridTrackSpec spec;
    spec.fr_units = std::max(
        std::strtof(trimmed.substr(0, trimmed.size() - 2).c_str(), nullptr),
        0.0f);
    if (spec.fr_units == 0.0f) {
      spec.fr_units = 1.0f;
    }
    return spec;
  }

  GridTrackSpec spec;
  const float fixed = parse_constraint_length(trimmed, container_size, elem);
  if (!std::isnan(fixed)) {
    spec.base_size = fixed;
  }
  return spec;
}

float min_auto_repeat_pattern_size(const std::vector<std::string>& tokens,
                                   float container_size,
                                   float gap,
                                   const Element* elem = nullptr) {
  if (tokens.empty()) {
    return 0.0f;
  }

  float total = 0.0f;
  for (const auto& token : tokens) {
    total += parse_grid_track_spec(token, container_size, elem).base_size;
  }
  total += gap * static_cast<float>(tokens.size() > 0 ? tokens.size() - 1 : 0);
  return total;
}

std::vector<std::string> expand_repeat_track_token(const std::string& token,
                                                   float container_size,
                                                   float gap,
                                                   const Element* elem = nullptr,
                                                   std::optional<size_t> auto_fit_limit =
                                                       std::nullopt) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.rfind("repeat(", 0) != 0 || trimmed.back() != ')') {
    return {trimmed};
  }

  const std::string inner = trimmed.substr(7, trimmed.size() - 8);
  int depth = 0;
  size_t comma_pos = std::string::npos;
  for (size_t i = 0; i < inner.size(); ++i) {
    if (inner[i] == '(') {
      ++depth;
    } else if (inner[i] == ')') {
      depth = std::max(depth - 1, 0);
    } else if (inner[i] == ',' && depth == 0) {
      comma_pos = i;
      break;
    }
  }

  if (comma_pos == std::string::npos) {
    return {trimmed};
  }

  const std::string count_text = trim_copy(inner.substr(0, comma_pos));
  const std::string pattern = trim_copy(inner.substr(comma_pos + 1));
  const auto pattern_tokens = split_grid_track_tokens(pattern);
  if (pattern_tokens.empty()) {
    return {};
  }

  int count = 0;
  const std::string lowered_count = trim_copy(count_text);
  if (lowered_count == "auto-fit" || lowered_count == "auto-fill") {
    const float min_pattern =
        min_auto_repeat_pattern_size(pattern_tokens, container_size, gap, elem);
    if (min_pattern <= 0.0f) {
      count = 1;
    } else {
      count = std::max(
          static_cast<int>(
              std::floor((container_size + gap) / (min_pattern + gap))),
          1);
    }
    if (lowered_count == "auto-fit" && auto_fit_limit.has_value()) {
      count = std::max(1, std::min(count, static_cast<int>(*auto_fit_limit)));
    }
  } else {
    count = std::max(std::atoi(count_text.c_str()), 0);
  }

  if (count <= 0) {
    return {};
  }

  std::vector<std::string> expanded;
  expanded.reserve(static_cast<size_t>(count) * pattern_tokens.size());
  for (int i = 0; i < count; ++i) {
    expanded.insert(expanded.end(), pattern_tokens.begin(), pattern_tokens.end());
  }
  return expanded;
}

std::vector<float> parse_grid_tracks(const std::string& value,
                                     float container_size,
                                     float gap,
                                     const Element* elem = nullptr,
                                     std::optional<size_t> auto_fit_limit =
                                         std::nullopt) {
  std::vector<std::string> tokens;
  for (const auto& token : split_grid_track_tokens(value)) {
    auto expanded =
        expand_repeat_track_token(token, container_size, gap, elem, auto_fit_limit);
    tokens.insert(tokens.end(), expanded.begin(), expanded.end());
  }

  if (tokens.empty()) {
    return {};
  }

  std::vector<float> tracks(tokens.size(), 0.0f);
  float fixed_total = 0.0f;
  float fr_units = 0.0f;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const GridTrackSpec spec = parse_grid_track_spec(tokens[i], container_size, elem);
    tracks[i] = spec.base_size;
    fixed_total += spec.base_size;
    fr_units += spec.fr_units;
  }

  const float total_gap = gap * static_cast<float>(tracks.size() > 0 ? tracks.size() - 1 : 0);
  const float remaining = std::max(container_size - fixed_total - total_gap, 0.0f);
  const float fr_unit_size = fr_units > 0.0f ? remaining / fr_units : 0.0f;
  for (size_t i = 0; i < tracks.size(); ++i) {
    const GridTrackSpec spec = parse_grid_track_spec(tokens[i], container_size, elem);
    tracks[i] = spec.base_size + spec.fr_units * fr_unit_size;
  }
  return tracks;
}

struct GridPlacement {
  int row = 0;
  int column = 0;
  int row_span = 1;
  int column_span = 1;
};

struct GridAreaPlacement {
  int row = 0;
  int column = 0;
  int row_span = 1;
  int column_span = 1;
};

enum class GridAxisAlignment {
  Start,
  End,
  Center,
  Stretch,
};

struct GridTrackDistribution {
  float start_offset = 0.0f;
  float gap = 0.0f;
};

void parse_grid_axis_value(const std::string& raw, int& start, int& span) {
  const std::string value = trim_copy(raw);
  if (value.empty() || value == "auto") {
    return;
  }

  if (value.find('/') != std::string::npos) {
    const size_t slash = value.find('/');
    const std::string left = trim_copy(value.substr(0, slash));
    const std::string right = trim_copy(value.substr(slash + 1));
    if (left.rfind("span ", 0) == 0) {
      span = std::max(std::atoi(trim_copy(left.substr(5)).c_str()), 1);
    } else if (!left.empty()) {
      start = std::max(std::atoi(left.c_str()) - 1, 0);
    }
    if (right.rfind("span ", 0) == 0) {
      span = std::max(std::atoi(trim_copy(right.substr(5)).c_str()), 1);
    } else if (!right.empty() && start >= 0) {
      span = std::max(std::atoi(right.c_str()) - 1 - start, 1);
    }
    return;
  }

  if (value.rfind("span ", 0) == 0) {
    span = std::max(std::atoi(trim_copy(value.substr(5)).c_str()), 1);
    return;
  }

  start = std::max(std::atoi(value.c_str()) - 1, 0);
}

GridAxisAlignment parse_grid_axis_alignment(const std::string& raw,
                                           GridAxisAlignment fallback) {
  const std::string value = trim_copy(raw);
  if (value.empty() || value == "auto") {
    return fallback;
  }

  if (value == "flex-start" || value == "start" || value == "self-start" ||
      value == "normal") {
    return GridAxisAlignment::Start;
  }
  if (value == "flex-end" || value == "end" || value == "self-end") {
    return GridAxisAlignment::End;
  }
  if (value == "center") {
    return GridAxisAlignment::Center;
  }
  if (value == "stretch") {
    return GridAxisAlignment::Stretch;
  }
  return fallback;
}

JustifyContent parse_grid_content_alignment(const std::string& raw,
                                            JustifyContent fallback) {
  const std::string value = trim_copy(raw);
  if (value.empty()) {
    return fallback;
  }

  if (value == "flex-start" || value == "start") {
    return JustifyContent::Start;
  }
  if (value == "flex-end" || value == "end") {
    return JustifyContent::End;
  }
  if (value == "center") {
    return JustifyContent::Center;
  }
  if (value == "space-between") {
    return JustifyContent::SpaceBetween;
  }
  if (value == "space-around") {
    return JustifyContent::SpaceAround;
  }
  if (value == "space-evenly") {
    return JustifyContent::SpaceEvenly;
  }
  return fallback;
}

GridTrackDistribution resolve_grid_track_distribution(JustifyContent mode,
                                                      float container_size,
                                                      float track_total,
                                                      size_t track_count,
                                                      float base_gap) {
  GridTrackDistribution distribution{0.0f, base_gap};
  if (track_count == 0) {
    return distribution;
  }

  const float base_total =
      track_total + base_gap * static_cast<float>(track_count > 0 ? track_count - 1 : 0);
  const float extra_space = std::max(container_size - base_total, 0.0f);

  switch (mode) {
    case JustifyContent::End:
      distribution.start_offset = extra_space;
      break;
    case JustifyContent::Center:
      distribution.start_offset = extra_space * 0.5f;
      break;
    case JustifyContent::SpaceBetween:
      if (track_count > 1) {
        distribution.gap += extra_space / static_cast<float>(track_count - 1);
      }
      break;
    case JustifyContent::SpaceAround:
      distribution.gap += extra_space / static_cast<float>(track_count);
      distribution.start_offset = distribution.gap * 0.5f;
      break;
    case JustifyContent::SpaceEvenly: {
      const float spacing = extra_space / static_cast<float>(track_count + 1);
      distribution.start_offset = spacing;
      distribution.gap += spacing;
      break;
    }
    case JustifyContent::Start:
    default:
      break;
  }

  return distribution;
}

GridPlacement parse_grid_item_placement(const ComputedStyle& style) {
  GridPlacement placement;

  parse_grid_axis_value(style.get_variable(Symbol("grid-column"), ""),
                        placement.column, placement.column_span);
  parse_grid_axis_value(style.get_variable(Symbol("grid-row"), ""),
                        placement.row, placement.row_span);

  const std::string area = trim_copy(style.get_variable(Symbol("grid-area"), ""));
  if (!area.empty() && area.find('/') != std::string::npos) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : area) {
      if (ch == '/') {
        parts.push_back(trim_copy(current));
        current.clear();
        continue;
      }
      current.push_back(ch);
    }
    parts.push_back(trim_copy(current));

    if (parts.size() >= 2) {
      parse_grid_axis_value(parts[0], placement.row, placement.row_span);
      parse_grid_axis_value(parts[1], placement.column, placement.column_span);
    }
    if (parts.size() >= 3) {
      const std::string& row_end = parts[2];
      if (row_end.rfind("span ", 0) == 0) {
        placement.row_span =
            std::max(std::atoi(trim_copy(row_end.substr(5)).c_str()), 1);
      } else if (!row_end.empty() && row_end != "auto") {
        const int row_end_index = std::max(std::atoi(row_end.c_str()) - 1, 0);
        placement.row_span =
            std::max(row_end_index - placement.row, 1);
      }
    }
    if (parts.size() >= 4) {
      const std::string& column_end = parts[3];
      if (column_end.rfind("span ", 0) == 0) {
        placement.column_span =
            std::max(std::atoi(trim_copy(column_end.substr(5)).c_str()), 1);
      } else if (!column_end.empty() && column_end != "auto") {
        const int column_end_index =
            std::max(std::atoi(column_end.c_str()) - 1, 0);
        placement.column_span =
            std::max(column_end_index - placement.column, 1);
      }
    }
  }
  return placement;
}

std::vector<std::string> parse_grid_area_row_tokens(const std::string& row_text) {
  std::vector<std::string> tokens;
  std::istringstream stream(row_text);
  std::string token;
  while (stream >> token) {
    tokens.push_back(trim_copy(token));
  }
  return tokens;
}

std::unordered_map<std::string, GridAreaPlacement> parse_grid_template_areas(
    const std::string& raw) {
  std::unordered_map<std::string, GridAreaPlacement> areas;
  std::vector<std::vector<std::string>> rows;

  bool in_quote = false;
  std::string current;
  for (char ch : raw) {
    if (ch == '"') {
      if (in_quote) {
        auto tokens = parse_grid_area_row_tokens(current);
        if (!tokens.empty()) {
          rows.push_back(std::move(tokens));
        }
        current.clear();
      }
      in_quote = !in_quote;
      continue;
    }
    if (in_quote) {
      current.push_back(ch);
    }
  }

  for (size_t row = 0; row < rows.size(); ++row) {
    for (size_t column = 0; column < rows[row].size(); ++column) {
      const std::string& token = rows[row][column];
      if (token.empty() || token == ".") {
        continue;
      }

      auto [it, inserted] = areas.emplace(
          token, GridAreaPlacement{static_cast<int>(row), static_cast<int>(column), 1, 1});
      if (inserted) {
        continue;
      }

      GridAreaPlacement& area = it->second;
      const int max_row = std::max(area.row + area.row_span - 1, static_cast<int>(row));
      const int max_column =
          std::max(area.column + area.column_span - 1, static_cast<int>(column));
      area.row = std::min(area.row, static_cast<int>(row));
      area.column = std::min(area.column, static_cast<int>(column));
      area.row_span = max_row - area.row + 1;
      area.column_span = max_column - area.column + 1;
    }
  }

  return areas;
}

void apply_grid_layout(Element* elem) {
  if (!elem || !elem->computed_style ||
      elem->computed_style->display != Display::Grid) {
    return;
  }

  auto* style = elem->computed_style;
  float row_gap = style->gap;
  if (!style->get_variable(Symbol("row-gap"), "").empty()) {
    const float parsed =
        parse_constraint_length(style->get_variable(Symbol("row-gap"), ""),
                                elem->layout_height(), elem);
    if (!std::isnan(parsed)) {
      row_gap = parsed;
    }
  }
  float column_gap = style->gap;
  if (!style->get_variable(Symbol("column-gap"), "").empty()) {
    const float parsed =
        parse_constraint_length(style->get_variable(Symbol("column-gap"), ""),
                                elem->layout_width(), elem);
    if (!std::isnan(parsed)) {
      column_gap = parsed;
    }
  }

  const float content_x = style->padding[3] + style->border_width[3];
  const float content_y = style->padding[0] + style->border_width[0];
  const float content_w =
      std::max(0.0f, elem->layout_width() - style->padding[1] - style->padding[3] -
                           style->border_width[1] - style->border_width[3]);
  const float content_h =
      std::max(0.0f, elem->layout_height() - style->padding[0] - style->padding[2] -
                           style->border_width[0] - style->border_width[2]);

  size_t in_flow_child_count = 0;
  for (auto* node : elem->children()) {
    auto* child = static_cast<Element*>(node);
    if (!child || !child->is_visible() || !child->computed_style) {
      continue;
    }
    if (child->computed_style->position == Position::Absolute ||
        child->computed_style->position == Position::Fixed) {
      continue;
    }
    ++in_flow_child_count;
  }

  auto columns =
      parse_grid_tracks(style->get_variable(Symbol("grid-template-columns"), ""),
                        content_w, column_gap, elem, in_flow_child_count);

  auto template_rows = parse_grid_tracks(style->get_variable(Symbol("grid-template-rows"), ""),
                                         content_h, row_gap, elem);
  const std::string auto_flow =
      trim_copy(style->get_variable(Symbol("grid-auto-flow"), ""));
  const bool auto_flow_column =
      auto_flow.find("column") != std::string::npos;
  const bool auto_flow_dense =
      auto_flow.find("dense") != std::string::npos;
  const auto auto_columns =
      parse_grid_tracks(style->get_variable(Symbol("grid-auto-columns"), ""),
                        content_w, column_gap, elem);
  const auto auto_rows =
      parse_grid_tracks(style->get_variable(Symbol("grid-auto-rows"), ""),
                        content_h, row_gap, elem);
  const float implicit_column_size =
      !auto_columns.empty() ? auto_columns.front() : content_w;
  const float implicit_row_size =
      !auto_rows.empty() ? auto_rows.front() : 0.0f;
  if (columns.empty()) {
    columns.push_back(implicit_column_size);
  }
  const GridAxisAlignment container_align =
      parse_grid_axis_alignment(style->get_variable(Symbol("align-items"), ""),
                                GridAxisAlignment::Stretch);
  const GridAxisAlignment container_justify =
      parse_grid_axis_alignment(style->get_variable(Symbol("justify-items"), ""),
                                GridAxisAlignment::Stretch);
  const auto named_areas =
      parse_grid_template_areas(style->get_variable(Symbol("grid-template-areas"), ""));
  const JustifyContent row_distribution_mode =
      parse_grid_content_alignment(style->get_variable(Symbol("align-content"), ""),
                                   JustifyContent::Start);
  const JustifyContent column_distribution_mode = style->justify_content;

  struct ChildPlacement {
    Element* child = nullptr;
    GridPlacement placement;
  };

  std::vector<ChildPlacement> placed_children;
  const size_t initial_row_count = std::max<size_t>(template_rows.size(), 1);
  std::vector<std::vector<bool>> occupied(
      initial_row_count, std::vector<bool>(columns.size(), false));

  auto ensure_rows = [&](int rows) {
    while (static_cast<int>(occupied.size()) < rows) {
      occupied.emplace_back(columns.size(), false);
    }
  };
  auto ensure_columns = [&](int cols) {
    while (static_cast<int>(columns.size()) < cols) {
      columns.push_back(implicit_column_size);
      for (auto& row : occupied) {
        row.push_back(false);
      }
    }
  };

  auto can_place = [&](int row, int column, int row_span, int column_span,
                       bool grow_rows, bool grow_columns) {
    if (row < 0 || column < 0) {
      return false;
    }

    if (grow_rows) {
      ensure_rows(row + row_span);
    } else if (row_span > static_cast<int>(occupied.size())) {
      ensure_rows(row_span);
    } else if (row + row_span > static_cast<int>(occupied.size())) {
      return false;
    }

    if (grow_columns) {
      ensure_columns(column + column_span);
    } else if (column_span > static_cast<int>(columns.size())) {
      ensure_columns(column_span);
    } else if (column + column_span > static_cast<int>(columns.size())) {
      return false;
    }

    for (int r = row; r < row + row_span; ++r) {
      for (int c = column; c < column + column_span; ++c) {
        if (occupied[r][c]) {
          return false;
        }
      }
    }
    return true;
  };

  auto mark_occupied = [&](int row, int column, int row_span, int column_span) {
    ensure_rows(row + row_span);
    ensure_columns(column + column_span);
    for (int r = row; r < row + row_span; ++r) {
      for (int c = column; c < column + column_span; ++c) {
        occupied[r][c] = true;
      }
    }
  };

  int auto_cursor_row = 0;
  int auto_cursor_column = 0;
  auto advance_auto_cursor = [&](const GridPlacement& placement) {
    if (auto_flow_dense) {
      return;
    }

    if (auto_flow_column) {
      auto_cursor_column = placement.column;
      auto_cursor_row = placement.row + std::max(placement.row_span, 1);
      while (!occupied.empty() &&
             auto_cursor_row >= static_cast<int>(occupied.size())) {
        auto_cursor_row -= static_cast<int>(occupied.size());
        ++auto_cursor_column;
      }
    } else {
      auto_cursor_row = placement.row;
      auto_cursor_column = placement.column + std::max(placement.column_span, 1);
      while (!columns.empty() &&
             auto_cursor_column >= static_cast<int>(columns.size())) {
        auto_cursor_column -= static_cast<int>(columns.size());
        ++auto_cursor_row;
      }
    }
  };

  for (auto* node : elem->children()) {
    auto* child = static_cast<Element*>(node);
    if (!child || !child->is_visible()) {
      continue;
    }
    if (child->computed_style &&
        (child->computed_style->position == Position::Absolute ||
         child->computed_style->position == Position::Fixed)) {
      continue;
    }

    GridPlacement placement = parse_grid_item_placement(*child->computed_style);
    const std::string area_name =
        trim_copy(child->computed_style->get_variable(Symbol("grid-area"), ""));
    if (!area_name.empty()) {
      auto area_it = named_areas.find(area_name);
      if (area_it != named_areas.end()) {
        placement.row = area_it->second.row;
        placement.column = area_it->second.column;
        placement.row_span = area_it->second.row_span;
        placement.column_span = area_it->second.column_span;
      }
    }
    placement.column_span = std::min(placement.column_span,
                                     static_cast<int>(columns.size()));

    const bool has_explicit_grid_placement =
        !child->computed_style->get_variable(Symbol("grid-column"), "").empty() ||
        !child->computed_style->get_variable(Symbol("grid-row"), "").empty() ||
        !child->computed_style->get_variable(Symbol("grid-area"), "").empty();

    const bool explicit_position_available =
        has_explicit_grid_placement &&
        can_place(placement.row, placement.column, placement.row_span,
                  placement.column_span, true, true);
    bool auto_placed = false;
    if (!explicit_position_available) {
      bool found = false;
      const int start_row = auto_flow_dense ? 0 : auto_cursor_row;
      const int start_column = auto_flow_dense ? 0 : auto_cursor_column;
      if (auto_flow_column) {
        for (int col = start_column; !found; ++col) {
          ensure_columns(col + 1);
          const int row_begin = col == start_column ? start_row : 0;
          for (int row = row_begin; row < static_cast<int>(occupied.size()); ++row) {
            if (can_place(row, col, placement.row_span, placement.column_span,
                          false, true)) {
              placement.row = row;
              placement.column = col;
              found = true;
              break;
            }
          }
          if (!found && occupied.empty()) {
            ensure_rows(1);
          }
        }
      } else {
        for (int row = start_row; !found; ++row) {
          ensure_rows(row + 1);
          const int column_begin = row == start_row ? start_column : 0;
          for (int col = column_begin; col < static_cast<int>(columns.size()); ++col) {
            if (can_place(row, col, placement.row_span, placement.column_span,
                          true, false)) {
              placement.row = row;
              placement.column = col;
              found = true;
              break;
            }
          }
        }
      }
      auto_placed = true;
    }

    mark_occupied(placement.row, placement.column,
                  placement.row_span, placement.column_span);
    if (auto_placed) {
      advance_auto_cursor(placement);
    }
    placed_children.push_back({child, placement});
  }

  std::vector<float> row_heights = template_rows;
  if (row_heights.size() < occupied.size()) {
    row_heights.resize(occupied.size(), implicit_row_size);
  }

  for (const auto& entry : placed_children) {
    auto* child = entry.child;
    auto* child_style = child->computed_style;
    const float child_total_height =
        child->layout_height() + child_style->margin[0] + child_style->margin[2];
    if (entry.placement.row_span <= 1) {
      row_heights[entry.placement.row] =
          std::max(row_heights[entry.placement.row], child_total_height);
    } else {
      const float per_row = child_total_height / entry.placement.row_span;
      for (int i = 0; i < entry.placement.row_span; ++i) {
        row_heights[entry.placement.row + i] =
            std::max(row_heights[entry.placement.row + i], per_row);
      }
    }
  }

  float total_column_width = 0.0f;
  for (float column : columns) {
    total_column_width += column;
  }
  const GridTrackDistribution column_distribution =
      resolve_grid_track_distribution(column_distribution_mode, content_w,
                                      total_column_width, columns.size(), column_gap);
  std::vector<float> column_offsets(columns.size(), 0.0f);
  float cursor_x = content_x + column_distribution.start_offset;
  for (size_t i = 0; i < columns.size(); ++i) {
    column_offsets[i] = cursor_x;
    cursor_x += columns[i] +
                (i + 1 < columns.size() ? column_distribution.gap : 0.0f);
  }

  float total_row_height = 0.0f;
  for (float row_height : row_heights) {
    total_row_height += row_height;
  }
  const GridTrackDistribution row_distribution =
      resolve_grid_track_distribution(row_distribution_mode, content_h,
                                      total_row_height, row_heights.size(), row_gap);
  std::vector<float> row_offsets(row_heights.size(), 0.0f);
  float cursor_y = content_y + row_distribution.start_offset;
  for (size_t i = 0; i < row_heights.size(); ++i) {
    row_offsets[i] = cursor_y;
    cursor_y += row_heights[i] +
                (i + 1 < row_heights.size() ? row_distribution.gap : 0.0f);
  }

  for (const auto& entry : placed_children) {
    auto* child = entry.child;
    auto* child_style = child->computed_style;
    float x = column_offsets[static_cast<size_t>(entry.placement.column)];

    float width = 0.0f;
    for (int i = 0; i < entry.placement.column_span; ++i) {
      width += columns[static_cast<size_t>(entry.placement.column + i)];
    }
    width += column_distribution.gap * std::max(entry.placement.column_span - 1, 0);

    float height = 0.0f;
    for (int i = 0; i < entry.placement.row_span; ++i) {
      height += row_heights[static_cast<size_t>(entry.placement.row + i)];
    }
    height += row_distribution.gap * std::max(entry.placement.row_span - 1, 0);

    const float horizontal_margin = child_style->margin[1] + child_style->margin[3];
    const float vertical_margin = child_style->margin[0] + child_style->margin[2];
    const float available_width = std::max(width - horizontal_margin, 0.0f);
    const float available_height = std::max(height - vertical_margin, 0.0f);
    const bool auto_width =
        child_style->width_size.kind == CssSizeKind::Auto;
    const bool auto_height =
        child_style->height_size.kind == CssSizeKind::Auto;
    const bool stretchable_width =
        auto_width || child_style->display == Display::Block ||
        child_style->display == Display::Flex || child_style->display == Display::Grid;
    const bool stretchable_height =
        auto_height || child_style->display == Display::Block ||
        child_style->display == Display::Flex || child_style->display == Display::Grid;

    const GridAxisAlignment justify_self = parse_grid_axis_alignment(
        child_style->get_variable(Symbol("justify-self"), ""), container_justify);
    const GridAxisAlignment align_self = parse_grid_axis_alignment(
        child_style->get_variable(Symbol("align-self"), ""), container_align);

    float resolved_width = child->layout_width();
    float resolved_height = child->layout_height();

    if (justify_self == GridAxisAlignment::Stretch && stretchable_width) {
      resolved_width = available_width;
      child->set_layout_width(available_width);
    }
    if (align_self == GridAxisAlignment::Stretch && stretchable_height) {
      resolved_height = available_height;
      child->set_layout_height(available_height);
    }

    resolved_width = std::max(0.0f, resolved_width);
    resolved_height = std::max(0.0f, resolved_height);

    GridAxisAlignment physical_justify_self = justify_self;
    if (style->direction == Direction::Rtl) {
      if (physical_justify_self == GridAxisAlignment::Start) {
        physical_justify_self = GridAxisAlignment::End;
      } else if (physical_justify_self == GridAxisAlignment::End) {
        physical_justify_self = GridAxisAlignment::Start;
      }
    }

    float child_x = x + child_style->margin[3];
    if (physical_justify_self == GridAxisAlignment::Center) {
      child_x += std::max(available_width - resolved_width, 0.0f) * 0.5f;
    } else if (physical_justify_self == GridAxisAlignment::End) {
      child_x += std::max(available_width - resolved_width, 0.0f);
    }

    float child_y = row_offsets[static_cast<size_t>(entry.placement.row)] +
                    child_style->margin[0];
    if (align_self == GridAxisAlignment::Center) {
      child_y += std::max(available_height - resolved_height, 0.0f) * 0.5f;
    } else if (align_self == GridAxisAlignment::End) {
      child_y += std::max(available_height - resolved_height, 0.0f);
    }

    child->set_x(child_x);
    child->set_y(child_y);
  }
}

} // namespace

void LayoutManager::sync_to_flex_impl(Element* elem, float container_w,
                                      float container_h,
                                      std::optional<float> used_width,
                                      std::optional<float> used_height) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (!style) return;
  auto* parent = elem->parent_elem();
  auto* parent_style = parent ? parent->computed_style : nullptr;

  const bool parent_is_flex = parent_style && parent_style->display == Display::Flex;
  const bool parent_row_flex = parent_is_flex &&
                               (parent_style->flex_direction == FlexDirection::Row ||
                                parent_style->flex_direction == FlexDirection::RowReverse);
  const bool parent_column_flex = parent_is_flex &&
                                  (parent_style->flex_direction == FlexDirection::Column ||
                                   parent_style->flex_direction == FlexDirection::ColumnReverse);

  // display:none
  if (style->display == Display::None) {
    elem->set_visible(false);
    return;
  }
  elem->set_visible(style->visibility == Visibility::Visible);
  elem->set_opacity(style->opacity);

  // Display mode
  if (style->display == Display::Flex) {
    elem->set_layout(flex::LayoutMode::Flex);
  } else if (style->display == Display::Block) {
    elem->set_layout(flex::LayoutMode::Flex);
    elem->set_flex_direction(flex::FlexDirection::Column);
  } else if (style->display == Display::Grid) {
    elem->set_layout(flex::LayoutMode::None);
  } else {
    elem->set_layout(flex::LayoutMode::None);
  }

  // Flex container properties
  if (style->display == Display::Flex) {
    FlexDirection effective_direction = style->flex_direction;
    if (style->direction == Direction::Rtl) {
      if (effective_direction == FlexDirection::Row) {
        effective_direction = FlexDirection::RowReverse;
      } else if (effective_direction == FlexDirection::RowReverse) {
        effective_direction = FlexDirection::Row;
      }
    }
    elem->set_flex_direction(effective_direction);
    elem->set_justify_content(style->justify_content);
    elem->set_align_items(style->align_items);
    const std::string flex_wrap = style->get_variable(Symbol("flex-wrap"), "");
    if (flex_wrap == "wrap") {
      elem->set_flex_wrap(FlexWrap::Wrap);
    } else {
      elem->set_flex_wrap(FlexWrap::NoWrap);
    }
  }
  float flex_gap = style->gap;
  if (style->display == Display::Flex) {
    const bool row_flow = style->flex_direction == FlexDirection::Row ||
                          style->flex_direction == FlexDirection::RowReverse;
    const Symbol gap_symbol = row_flow ? Symbol("column-gap") : Symbol("row-gap");
    const std::string gap_value = style->get_variable(gap_symbol, "");
    if (!gap_value.empty()) {
      const float parsed_gap =
          parse_constraint_length(gap_value, row_flow ? container_w : container_h,
                                  elem);
      if (!std::isnan(parsed_gap)) {
        flex_gap = parsed_gap;
      }
    }
  }
  elem->set_gap(flex_gap);
  elem->set_padding(style->padding[0], style->padding[1],
                    style->padding[2], style->padding[3]);

  // Flex item properties
  float flex_grow = style->get_variable_float(Symbol("flex-grow"), 0.0f);
  const float default_flex_shrink = parent_is_flex ? 1.0f : 0.0f;
  float flex_shrink =
      style->get_variable_float(Symbol("flex-shrink"), default_flex_shrink);
  const std::string flex_basis_value =
      style->get_variable(Symbol("flex-basis"), "auto");
  float flex_basis = style->get_variable_float(Symbol("flex-basis"), 0.0f);
  elem->set_flex(flex_grow, flex_shrink, flex_basis);
  elem->set_flex_basis_auto(flex_basis_value.empty() ||
                            flex_basis_value == "auto");
  auto align_self_value = style->get_variable(Symbol("align-self"), "");
  AlignSelf resolved_align_self = AlignSelf::Auto;
  if (align_self_value == "flex-start" || align_self_value == "start") {
    resolved_align_self = AlignSelf::Start;
  } else if (align_self_value == "flex-end" || align_self_value == "end") {
    resolved_align_self = AlignSelf::End;
  } else if (align_self_value == "center") {
    resolved_align_self = AlignSelf::Center;
  } else if (align_self_value == "stretch") {
    resolved_align_self = AlignSelf::Stretch;
  }
  if (resolved_align_self == AlignSelf::Auto && parent_style &&
      parent_style->align_items == AlignItems::Stretch) {
    const bool has_explicit_cross_size =
        parent_row_flex ? style->height_size.kind != CssSizeKind::Auto
                        : style->width_size.kind != CssSizeKind::Auto;
    if (has_explicit_cross_size) {
      resolved_align_self = AlignSelf::Start;
    }
  }
  elem->set_align_self(resolved_align_self);

  elem->set_position_mode(to_flex_position(style->position));
  elem->set_position_offsets(style->top, style->right, style->bottom, style->left);
  elem->set_z_index(style->z_index);
  elem->set_box_sizing(to_flex_box_sizing(style->box_sizing));
  elem->set_margin(style->margin[0], style->margin[1], style->margin[2], style->margin[3]);
  elem->set_border_width(style->border_width[0], style->border_width[1],
                         style->border_width[2], style->border_width[3]);

  const float horizontal_padding = style->padding[1] + style->padding[3];
  const float vertical_padding = style->padding[0] + style->padding[2];
  const float horizontal_border = style->border_width[1] + style->border_width[3];
  const float vertical_border = style->border_width[0] + style->border_width[2];
  const float horizontal_extras = horizontal_padding + horizontal_border;
  const float vertical_extras = vertical_padding + vertical_border;

  auto outer_width_for_spec = [&](float specified) {
    if (style->box_sizing == BoxSizing::BorderBox) {
      return specified;
    }
    return specified + horizontal_extras;
  };
  auto outer_height_for_spec = [&](float specified) {
    if (style->box_sizing == BoxSizing::BorderBox) {
      return specified;
    }
    return specified + vertical_extras;
  };
  auto parse_outer_constraint = [&](Symbol name, float container_size,
                                    auto&& outer_for_spec) -> std::optional<float> {
    const std::string raw = style->get_variable(name, "");
    if (raw.empty() || raw == "auto" || raw == "none") {
      return std::nullopt;
    }

    const float specified = parse_constraint_length(raw, container_size, elem);
    if (std::isnan(specified)) {
      return std::nullopt;
    }
    return outer_for_spec(specified);
  };

  const auto min_width = parse_outer_constraint(Symbol("min-width"), container_w, outer_width_for_spec);
  const auto max_width = parse_outer_constraint(Symbol("max-width"), container_w, outer_width_for_spec);
  const auto min_height =
      parse_outer_constraint(Symbol("min-height"), container_h, outer_height_for_spec);
  const auto max_height =
      parse_outer_constraint(Symbol("max-height"), container_h, outer_height_for_spec);
  const float aspect_ratio =
      parse_aspect_ratio_value(style->get_variable(Symbol("aspect-ratio"), ""));

  auto height_from_width = [&](float outer_width) {
    if (!(aspect_ratio > 0.0f)) {
      return outer_width;
    }
    if (style->box_sizing == BoxSizing::BorderBox) {
      return outer_width / aspect_ratio;
    }
    const float content_width = std::max(outer_width - horizontal_extras, 0.0f);
    return outer_height_for_spec(content_width / aspect_ratio);
  };

  auto width_from_height = [&](float outer_height) {
    if (!(aspect_ratio > 0.0f)) {
      return outer_height;
    }
    if (style->box_sizing == BoxSizing::BorderBox) {
      return outer_height * aspect_ratio;
    }
    const float content_height = std::max(outer_height - vertical_extras, 0.0f);
    return outer_width_for_spec(content_height * aspect_ratio);
  };

  // Width
  float width = 0;
  bool auto_width = false;
  if (style->width_size.kind == CssSizeKind::Percentage) {
    width = outer_width_for_spec(container_w * style->width / 100.0f);
    elem->set_layout_width(width);
  } else if (style->width_size.kind == CssSizeKind::Expression) {
    width = outer_width_for_spec(parse_constraint_length(
        style->width_size.expression, container_w, elem));
    elem->set_layout_width(width);
  } else if (style->width_size.kind != CssSizeKind::Auto) {
    width = outer_width_for_spec(style->width);
    elem->set_layout_width(width);
  } else {
    auto_width = true;
    width = parent_row_flex ? 0.0f : container_w;
    if (style->display == Display::Block && !parent_row_flex) {
      elem->set_layout_width(width);
    } else {
      elem->set_layout_width(0);
    }
  }

  // Height
  float height = 0;
  bool auto_height = false;
  if (style->height_size.kind == CssSizeKind::Percentage) {
    height = outer_height_for_spec(container_h * style->height / 100.0f);
    elem->set_layout_height(height);
  } else if (style->height_size.kind == CssSizeKind::Expression) {
    height = outer_height_for_spec(parse_constraint_length(
        style->height_size.expression, container_h, elem));
    elem->set_layout_height(height);
  } else if (style->height_size.kind != CssSizeKind::Auto) {
    height = outer_height_for_spec(style->height);
    elem->set_layout_height(height);
  } else {
    auto_height = true;
    if (style->display == Display::Block && parent_column_flex) {
      elem->set_layout_height(0);
    }
    height = elem->layout_height();
  }

  if (aspect_ratio > 0.0f) {
    if (!auto_width && auto_height) {
      height = height_from_width(width);
      auto_height = false;
      elem->set_layout_height(height);
    } else if (auto_width && !auto_height) {
      width = width_from_height(height);
      auto_width = false;
      elem->set_layout_width(width);
    }
  }

  if (used_width.has_value()) {
    width = *used_width;
    auto_width = false;
  }
  if (used_height.has_value()) {
    height = *used_height;
    auto_height = false;
  }

  width = clamp_dimension(width, min_width, max_width);
  height = clamp_dimension(height, min_height, max_height);
  elem->set_layout_width(width);
  elem->set_layout_height(height);

  // Content area for children
  float content_w = std::max(0.0f, width - horizontal_extras);
  float content_h = std::max(0.0f, height - vertical_extras);

  float intrinsic_width = 0.0f;
  float intrinsic_height = 0.0f;
  const bool has_application_child = std::any_of(
      elem->children().begin(), elem->children().end(), [](const flex::Node* node) {
        const auto* child = dynamic_cast<const Element*>(node);
        return child && !child->is_widget_owned();
      });
  const bool is_widget_leaf = elem->widget != nullptr && !has_application_child;
  bool has_widget_intrinsic = false;
  if (is_widget_leaf) {
    const float available_widget_width =
        content_w > 0.0f ? content_w : std::max(container_w - horizontal_extras, 0.0f);
    const float available_widget_height =
        content_h > 0.0f ? content_h : std::max(container_h - vertical_extras, 0.0f);
    has_widget_intrinsic = elem->widget->measure_intrinsic_size(
        *elem, available_widget_width, available_widget_height, intrinsic_width,
        intrinsic_height);
  }

  // Recurse children first (for auto sizing)
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      LayoutManager::sync_to_flex_impl(child, content_w, content_h);
    }
  }

  // Auto height from children
  if (auto_height) {
    float children_height = 0;
    bool is_row = (style->display == Display::Flex) &&
                  (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);
    const bool wraps = style->display == Display::Flex &&
                       style->get_variable(Symbol("flex-wrap"), "") == "wrap";
    int in_flow_count = 0;

    for (auto* node : elem->children()) {
      auto* child = static_cast<Element*>(node);
      if (!child || !child->is_visible()) continue;

      auto* cs = child->computed_style;
      if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
        continue;
      }

      float h = child->layout_height();
      if (cs) h += cs->margin[0] + cs->margin[2];

      if (is_row) {
        children_height = std::max(children_height, h);
      } else {
        children_height += h;
      }
      in_flow_count++;
    }

    if (!is_row && in_flow_count > 1) {
      children_height += style->gap * (in_flow_count - 1);
    }

    if (is_row && wraps) {
      const float wrapped_cross = measure_wrapped_flex_cross_size(
          elem, true, style->gap);
      if (wrapped_cross > 0.0f) {
        children_height = wrapped_cross;
      }
    }

    float computed_height = children_height + vertical_extras;

    // Text leaves derive auto height from the shared text layout pipeline so
    // wrapping, explicit line breaks, white-space, and line-clamp stay aligned.
    if (has_widget_intrinsic && intrinsic_height > 0.0f) {
      computed_height =
          std::max(computed_height, intrinsic_height + vertical_extras);
    } else if (elem->children().empty() && !elem->text().empty()) {
      float available_text_width = content_w;
      if (!(available_text_width > 0.0f)) {
        available_text_width = std::max(container_w - horizontal_extras, 0.0f);
      }
      const float text_height =
          measure_text_leaf_height(style, elem->text(), available_text_width);
      computed_height = std::max(computed_height, text_height + vertical_extras);
    } else if (computed_height <= 0 && elem->children().empty()) {
      computed_height = style->font_size > 0 ? style->font_size : 24.0f;
    }

    computed_height = clamp_dimension(computed_height, min_height, max_height);
    elem->set_layout_height(computed_height);
  }

  // Auto width from children
  if (auto_width && !elem->children().empty()) {
    float children_width = 0;
    bool is_row = (style->display == Display::Flex) &&
                  (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);
    int in_flow_count = 0;

    for (auto* node : elem->children()) {
      auto* child = static_cast<Element*>(node);
      if (!child || !child->is_visible()) continue;

      auto* cs = child->computed_style;
      if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
        continue;
      }

      float w = child->layout_width();
      if (cs) w += cs->margin[1] + cs->margin[3];

      if (is_row) {
        children_width += w;
      } else {
        children_width = std::max(children_width, w);
      }
      in_flow_count++;
    }

    if (is_row && in_flow_count > 1) {
      children_width += style->gap * (in_flow_count - 1);
    }

    float computed_width = children_width + horizontal_extras;
    computed_width = clamp_dimension(computed_width, min_width, max_width);
    elem->set_layout_width(computed_width);
  }

  // Auto width for text leaves
  if (auto_width && has_widget_intrinsic && intrinsic_width > 0.0f) {
    float computed_width = intrinsic_width + horizontal_extras;
    computed_width = clamp_dimension(computed_width, min_width, max_width);
    elem->set_layout_width(computed_width);
  } else if (auto_width && elem->children().empty() && !elem->text().empty()) {
    const float fs = style->font_size > 0 ? style->font_size : 14.0f;
    const float text_width = approximate_segmented_text_width(style, elem->text());
    float computed_width = text_width + horizontal_extras;
    computed_width = std::max(computed_width, fs * 2);
    computed_width = clamp_dimension(computed_width, min_width, max_width);
    elem->set_layout_width(computed_width);
  }

  if (aspect_ratio > 0.0f) {
    if (!auto_width && auto_height) {
      float computed_height =
          clamp_dimension(height_from_width(elem->layout_width()), min_height, max_height);
      elem->set_layout_height(computed_height);
    } else if (auto_width && !auto_height) {
      float computed_width =
          clamp_dimension(width_from_height(elem->layout_height()), min_width, max_width);
      elem->set_layout_width(computed_width);
    }
  }

  const bool clip_overflow = style->overflow_x == Overflow::Hidden ||
                             style->overflow_y == Overflow::Hidden ||
                             is_scroll_overflow(style->overflow_x) ||
                             is_scroll_overflow(style->overflow_y);
  elem->set_clip(clip_overflow);
  if (clip_overflow) {
    elem->set_clip_size(elem->layout_width(), elem->layout_height());
  } else {
    elem->set_clip_size(0.0f, 0.0f);
  }
}

namespace {

struct ChildSizeBeforeLayout {
  Element* child = nullptr;
  float width = 0.0f;
  float height = 0.0f;
};

std::pair<std::optional<float>, std::optional<float>> used_child_size(
    const Element* parent, const Element* child, float width_before,
    float height_before) {
  if (!parent || !child || !parent->computed_style || !child->computed_style) {
    return {};
  }

  const auto* parent_style = parent->computed_style;
  const auto* child_style = child->computed_style;
  if (child_style->position == Position::Absolute ||
      child_style->position == Position::Fixed ||
      parent_style->display == Display::Grid) {
    return {child->layout_width(), child->layout_height()};
  }

  if (parent_style->display != Display::Flex) {
    return {};
  }

  constexpr float kLayoutEpsilon = 0.001f;
  const std::optional<float> width =
      std::fabs(child->layout_width() - width_before) > kLayoutEpsilon
          ? std::optional<float>(child->layout_width())
          : std::nullopt;
  const std::optional<float> height =
      std::fabs(child->layout_height() - height_before) > kLayoutEpsilon
          ? std::optional<float>(child->layout_height())
          : std::nullopt;
  return {width, height};
}

} // namespace

void LayoutManager::sync_to_flex(Element* elem, float container_w,
                                 float container_h) {
  LayoutManager::sync_to_flex_impl(elem, container_w, container_h);
}

void LayoutManager::perform_layout(Element* elem) {
  if (!elem) return;

  std::vector<ChildSizeBeforeLayout> child_sizes;
  child_sizes.reserve(elem->children().size());
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      child_sizes.push_back(
          {child, child->layout_width(), child->layout_height()});
    }
  }

  apply_layout_constraints(elem);
  if (elem->computed_style && elem->computed_style->display == Display::Grid) {
    apply_grid_layout(elem);
  } else {
    elem->perform_layout();
  }
  apply_layout_constraints(elem);

  float content_w = elem->layout_width();
  float content_h = elem->layout_height();
  if (elem->computed_style) {
    const auto* style = elem->computed_style;
    content_w = std::max(0.0f, content_w - style->padding[1] - style->padding[3] -
                                   style->border_width[1] - style->border_width[3]);
    content_h = std::max(0.0f, content_h - style->padding[0] - style->padding[2] -
                                   style->border_width[0] - style->border_width[2]);
  }

  for (const auto& before : child_sizes) {
    const auto [used_width, used_height] =
        used_child_size(elem, before.child, before.width, before.height);
    if (used_width.has_value() || used_height.has_value()) {
      LayoutManager::sync_to_flex_impl(before.child, content_w, content_h,
                                       used_width, used_height);
    }
    perform_layout(before.child);
  }

  update_scroll_metrics(elem);
}

} // namespace flexUI
