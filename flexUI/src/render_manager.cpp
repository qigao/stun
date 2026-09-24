/*
 * flexUI - RenderManager Implementation
 */

#include <flexUI/render_manager.h>
#include <flexUI/detail/style_property_registry.h>
#include <flexUI/element.h>
#include <flexUI/box.h>
#include <flexUI/renderer.h>
#include <flexUI/computed_style.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <flexUI/widget.h>
#include <flexUI/detail/css_length.h>
#include <flex/runtime/group.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <tlog.h>
#include <unordered_map>
#include <vector>

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

std::string to_lower_copy(const std::string& value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return lowered;
}

bool starts_with_case_insensitive(const std::string& value,
                                  const std::string& prefix) {
  if (value.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(value[i])) !=
        std::tolower(static_cast<unsigned char>(prefix[i]))) {
      return false;
    }
  }
  return true;
}

const Symbol& filter_drop_shadow_offset_x_var() {
  static const Symbol sym("__flex_filter_drop_shadow_offset_x");
  return sym;
}

const Symbol& filter_drop_shadow_offset_y_var() {
  static const Symbol sym("__flex_filter_drop_shadow_offset_y");
  return sym;
}

const Symbol& filter_drop_shadow_blur_var() {
  static const Symbol sym("__flex_filter_drop_shadow_blur");
  return sym;
}

const Symbol& filter_drop_shadow_color_var() {
  static const Symbol sym("__flex_filter_drop_shadow_color");
  return sym;
}

const Symbol& filter_opacity_var() {
  static const Symbol sym("__flex_filter_opacity");
  return sym;
}

const Symbol& background_origin_var() {
  static const Symbol sym("__flex_background_origin");
  return sym;
}

bool is_svg_source(const std::string& src) {
  const std::string trimmed = trim_copy(src);
  if (starts_with_case_insensitive(trimmed, "data:image/svg+xml") ||
      starts_with_case_insensitive(trimmed, "<svg")) {
    return true;
  }
  if (trimmed.size() < 4) return false;
  const size_t suffix_pos = trimmed.size() - 4;
  const char c0 = static_cast<char>(
      std::tolower(static_cast<unsigned char>(trimmed[suffix_pos + 0])));
  const char c1 = static_cast<char>(
      std::tolower(static_cast<unsigned char>(trimmed[suffix_pos + 1])));
  const char c2 = static_cast<char>(
      std::tolower(static_cast<unsigned char>(trimmed[suffix_pos + 2])));
  const char c3 = static_cast<char>(
      std::tolower(static_cast<unsigned char>(trimmed[suffix_pos + 3])));
  return c0 == '.' && c1 == 's' && c2 == 'v' && c3 == 'g';
}

RenderCommandList make_render_commands(
    const flex::RendererCapabilities& capabilities) {
  return RenderCommandList(capabilities);
}

RenderCommandList make_render_commands(
    const flex::RendererCapabilities& capabilities,
    const Transform& transform_prefix) {
  return RenderCommandList(capabilities, transform_prefix);
}

Color get_transition_color(TransitionManager& transitions,
                           std::uintptr_t element_id,
                           detail::StylePropertyId property_id,
                           const Color& fallback,
                           float current_time_ms) {
  const auto* property = detail::style_property_descriptor(property_id);
  return property
             ? transitions.get_color(element_id, *property, fallback,
                                     current_time_ms)
             : fallback;
}

float get_animation_float(const AnimationManager& animations,
                          std::uintptr_t element_id,
                          detail::StylePropertyId property_id,
                          float fallback,
                          float current_time_ms) {
  const auto* property = detail::style_property_descriptor(property_id);
  return property
             ? animations.get_float(element_id, *property, fallback,
                                    current_time_ms)
             : fallback;
}

struct RenderProfile {
  std::chrono::high_resolution_clock::time_point start{};
  double frame_start_ms = 0.0;
  double frame_end_ms = 0.0;
  double tree_ms = 0.0;
  double overlay_ms = 0.0;
  double replay_ms = 0.0;
  double element_ms = 0.0;
  double effect_ms = 0.0;
  double background_ms = 0.0;
  double stroke_ms = 0.0;
  double content_ms = 0.0;
  double pseudo_ms = 0.0;
  double widget_ms = 0.0;
  double text_ms = 0.0;
  double sort_ms = 0.0;
  int elements = 0;
  int widgets = 0;
  int text_blocks = 0;
  int command_count = 0;
  std::unordered_map<std::string, std::pair<int, double>> widget_detail;
};

thread_local RenderProfile* active_render_profile = nullptr;

bool render_profile_enabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("FLEXUI_RENDER_PROFILE");
    if (!value || value[0] == '\0') {
      return false;
    }
    const std::string lowered = to_lower_copy(value);
    return lowered == "1" || lowered == "true" || lowered == "on" ||
           lowered == "yes";
  }();
  return enabled;
}

double elapsed_ms(std::chrono::high_resolution_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::high_resolution_clock::now() - start)
      .count();
}

void add_profile_time(double& target,
                      std::chrono::high_resolution_clock::time_point start) {
  if (active_render_profile) {
    target += elapsed_ms(start);
  }
}

void replay_render_commands(Renderer& renderer,
                            const RenderCommandList& commands) {
  if (commands.commands().empty()) {
    return;
  }
  auto start = active_render_profile
                   ? std::chrono::high_resolution_clock::now()
                   : std::chrono::high_resolution_clock::time_point{};
  commands.replay(renderer);
  if (active_render_profile) {
    active_render_profile->replay_ms += elapsed_ms(start);
    active_render_profile->command_count +=
        static_cast<int>(commands.commands().size());
  }
}

int from_hex_digit(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

std::string url_decode_copy(const std::string& value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const int hi = from_hex_digit(value[i + 1]);
      const int lo = from_hex_digit(value[i + 2]);
      if (hi >= 0 && lo >= 0) {
        decoded.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    decoded.push_back(value[i] == '+' ? ' ' : value[i]);
  }
  return decoded;
}

detail::CssLengthContext make_length_context(const Element* elem,
                                             float reference) {
  detail::CssLengthContext context;
  context.percent_reference = reference;
  if (elem && elem->owner_box_) {
    context.viewport_width = elem->owner_box_->viewport_width();
    context.viewport_height = elem->owner_box_->viewport_height();
  }
  return context;
}

float parse_pseudo_length(const std::string& value, float reference,
                          const Element* elem = nullptr) {
  return detail::parse_css_length(value, make_length_context(elem, reference));
}

std::vector<std::string> split_pseudo_tokens(const std::string& value) {
  return detail::split_css_tokens(value);
}

std::array<float, 4> parse_pseudo_box_values(const std::string& value,
                                             float horizontal_ref,
                                             float vertical_ref,
                                             const Element* elem = nullptr) {
  std::array<float, 4> result{NAN, NAN, NAN, NAN};
  const auto tokens = split_pseudo_tokens(value);
  if (tokens.empty()) {
    return result;
  }

  auto parse_axis = [&](const std::string& token, bool horizontal) {
    return parse_pseudo_length(token, horizontal ? horizontal_ref : vertical_ref,
                               elem);
  };

  if (tokens.size() == 1) {
    result[0] = result[1] = result[2] = result[3] =
        parse_axis(tokens[0], false);
    result[1] = result[3] = parse_axis(tokens[0], true);
    return result;
  }
  if (tokens.size() == 2) {
    result[0] = result[2] = parse_axis(tokens[0], false);
    result[1] = result[3] = parse_axis(tokens[1], true);
    return result;
  }
  if (tokens.size() == 3) {
    result[0] = parse_axis(tokens[0], false);
    result[1] = result[3] = parse_axis(tokens[1], true);
    result[2] = parse_axis(tokens[2], false);
    return result;
  }

  result[0] = parse_axis(tokens[0], false);
  result[1] = parse_axis(tokens[1], true);
  result[2] = parse_axis(tokens[2], false);
  result[3] = parse_axis(tokens[3], true);
  return result;
}

float parse_pseudo_opacity(const std::string& value, float default_value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty()) {
    return default_value;
  }
  return std::clamp(std::strtof(trimmed.c_str(), nullptr), 0.0f, 1.0f);
}

std::vector<std::string> split_background_tokens(const std::string& value) {
  auto tokens = detail::split_css_tokens(value);
  for (auto& token : tokens) {
    token = to_lower_copy(token);
  }
  return tokens;
}

float resolve_fractional_axis(const std::string& token, float reference,
                              bool horizontal, float default_value,
                              const Element* elem = nullptr) {
  if (token.empty()) {
    return default_value;
  }
  if (token == "center") {
    return 0.5f;
  }
  if (horizontal) {
    if (token == "left") {
      return 0.0f;
    }
    if (token == "right") {
      return 1.0f;
    }
  } else {
    if (token == "top") {
      return 0.0f;
    }
    if (token == "bottom") {
      return 1.0f;
    }
  }
  if (!token.empty() && token.back() == '%') {
    return std::clamp(
        std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) / 100.0f,
        0.0f, 1.0f);
  }
  if (reference > 0.0f) {
    return std::clamp(parse_pseudo_length(token, reference, elem) / reference, 0.0f,
                      1.0f);
  }
  return default_value;
}

std::pair<float, float> resolve_radial_center(const std::string& value,
                                              float width, float height,
                                              const Element* elem = nullptr) {
  const auto tokens = split_background_tokens(value);
  if (tokens.empty()) {
    return {0.5f, 0.5f};
  }

  std::string x_token = "center";
  std::string y_token = "center";
  if (tokens.size() == 1) {
    const auto& token = tokens[0];
    if (token == "top" || token == "bottom") {
      y_token = token;
    } else {
      x_token = token;
    }
  } else {
    x_token = tokens[0];
    y_token = tokens[1];
  }

  return {
      resolve_fractional_axis(x_token, width, true, 0.5f, elem),
      resolve_fractional_axis(y_token, height, false, 0.5f, elem),
  };
}

bool is_background_horizontal_keyword(const std::string& token) {
  return token == "left" || token == "right";
}

bool is_background_vertical_keyword(const std::string& token) {
  return token == "top" || token == "bottom";
}

bool is_background_position_side_keyword(const std::string& token) {
  return is_background_horizontal_keyword(token) ||
         is_background_vertical_keyword(token);
}

bool is_background_position_offset_token(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  if (token.back() == '%') {
    return true;
  }
  if (is_background_position_side_keyword(token) || token == "center") {
    return false;
  }
  return true;
}

struct BackgroundAxisPosition {
  std::string anchor = "center";
  std::string offset;
};

float resolve_background_position_axis(const BackgroundAxisPosition& position,
                                       float free_space, bool horizontal,
                                       const Element* elem = nullptr) {
  const std::string& token = position.anchor;
  if (token.empty()) {
    return 0.0f;
  }
  if (!position.offset.empty()) {
    const float offset = position.offset.back() == '%'
                             ? free_space * std::strtof(
                                                position.offset
                                                    .substr(0, position.offset.size() - 1)
                                                    .c_str(),
                                                nullptr) /
                                   100.0f
                             : parse_pseudo_length(position.offset, free_space, elem);
    if ((horizontal && token == "right") ||
        (!horizontal && token == "bottom")) {
      return free_space - offset;
    }
    if (token == "center") {
      return free_space * 0.5f + offset;
    }
    return offset;
  }
  if (token == "center") {
    return free_space * 0.5f;
  }
  if (horizontal) {
    if (token == "left") {
      return 0.0f;
    }
    if (token == "right") {
      return free_space;
    }
  } else {
    if (token == "top") {
      return 0.0f;
    }
    if (token == "bottom") {
      return free_space;
    }
  }
  if (!token.empty() && token.back() == '%') {
    return free_space *
           std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) / 100.0f;
  }
  return parse_pseudo_length(token, free_space, elem);
}

std::pair<BackgroundAxisPosition, BackgroundAxisPosition>
parse_background_position_tokens(const std::vector<std::string>& tokens) {
  BackgroundAxisPosition x_axis;
  BackgroundAxisPosition y_axis;
  if (tokens.empty()) {
    return {x_axis, y_axis};
  }

  if (tokens.size() == 1) {
    const auto& token = tokens[0];
    if (is_background_vertical_keyword(token)) {
      y_axis.anchor = token;
    } else {
      x_axis.anchor = token;
    }
    return {x_axis, y_axis};
  }

  if (tokens.size() == 2) {
    const auto& first = tokens[0];
    const auto& second = tokens[1];
    if (is_background_vertical_keyword(first) &&
        (is_background_horizontal_keyword(second) || second == "center")) {
      x_axis.anchor = second;
      y_axis.anchor = first;
    } else if ((is_background_horizontal_keyword(first) || first == "center") &&
               is_background_vertical_keyword(second)) {
      x_axis.anchor = first;
      y_axis.anchor = second;
    } else {
      x_axis.anchor = first;
      y_axis.anchor = second;
    }
    return {x_axis, y_axis};
  }

  bool has_x = false;
  bool has_y = false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto& token = tokens[i];
    if (is_background_horizontal_keyword(token)) {
      x_axis.anchor = token;
      has_x = true;
      if (i + 1 < tokens.size() &&
          is_background_position_offset_token(tokens[i + 1])) {
        x_axis.offset = tokens[++i];
      }
      continue;
    }
    if (is_background_vertical_keyword(token)) {
      y_axis.anchor = token;
      has_y = true;
      if (i + 1 < tokens.size() &&
          is_background_position_offset_token(tokens[i + 1])) {
        y_axis.offset = tokens[++i];
      }
      continue;
    }
    if (token == "center") {
      if (!has_x) {
        x_axis.anchor = token;
        has_x = true;
      } else {
        y_axis.anchor = token;
        has_y = true;
      }
      continue;
    }
    if (!has_x) {
      x_axis.anchor = token;
      has_x = true;
    } else if (!has_y) {
      y_axis.anchor = token;
      has_y = true;
    }
  }
  return {x_axis, y_axis};
}

std::pair<float, float> resolve_background_position(const std::string& value,
                                                    float free_x, float free_y,
                                                    const Element* elem = nullptr) {
  const auto tokens = split_background_tokens(value);
  if (tokens.empty()) {
    return {0.0f, 0.0f};
  }

  const auto axes = parse_background_position_tokens(tokens);

  return {
      resolve_background_position_axis(axes.first, free_x, true, elem),
      resolve_background_position_axis(axes.second, free_y, false, elem),
  };
}

struct BackgroundIntrinsicMetrics {
  bool has_aspect_ratio = false;
  float aspect_ratio = 1.0f;
};

std::string extract_svg_markup_from_source(const std::string& src) {
  const std::string trimmed = trim_copy(src);
  if (trimmed.empty()) {
    return {};
  }
  if (starts_with_case_insensitive(trimmed, "<svg")) {
    return trimmed;
  }
  if (!starts_with_case_insensitive(trimmed, "data:image/svg+xml")) {
    return {};
  }

  const size_t comma = trimmed.find(',');
  if (comma == std::string::npos || comma + 1 >= trimmed.size()) {
    return {};
  }

  const std::string metadata = to_lower_copy(trimmed.substr(0, comma));
  if (metadata.find(";base64") != std::string::npos) {
    return {};
  }
  return url_decode_copy(trimmed.substr(comma + 1));
}

std::string extract_svg_attribute(const std::string& markup,
                                  const std::string& name) {
  const size_t svg_start = markup.find("<svg");
  if (svg_start == std::string::npos) {
    return {};
  }
  const size_t tag_end = markup.find('>', svg_start);
  const std::string tag = markup.substr(
      svg_start, tag_end == std::string::npos ? std::string::npos
                                              : (tag_end - svg_start));
  const std::string needle = name + "=";
  size_t search_pos = 0;
  while ((search_pos = tag.find(needle, search_pos)) != std::string::npos) {
    if (search_pos > 0) {
      const char previous = tag[search_pos - 1];
      if (std::isalnum(static_cast<unsigned char>(previous)) || previous == '-' ||
          previous == ':') {
        search_pos += needle.size();
        continue;
      }
    }

    size_t value_start = search_pos + needle.size();
    if (value_start >= tag.size()) {
      return {};
    }
    const char quote = tag[value_start];
    if (quote == '"' || quote == '\'') {
      ++value_start;
      const size_t value_end = tag.find(quote, value_start);
      if (value_end == std::string::npos) {
        return {};
      }
      return tag.substr(value_start, value_end - value_start);
    }

    size_t value_end = value_start;
    while (value_end < tag.size() &&
           !std::isspace(static_cast<unsigned char>(tag[value_end])) &&
           tag[value_end] != '/') {
      ++value_end;
    }
    return tag.substr(value_start, value_end - value_start);
  }
  return {};
}

float parse_svg_length(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty() || trimmed.back() == '%') {
    return 0.0f;
  }

  char* end = nullptr;
  const float parsed = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || !std::isfinite(parsed) || parsed <= 0.0f) {
    return 0.0f;
  }
  return parsed;
}

BackgroundIntrinsicMetrics resolve_background_image_intrinsic_metrics(
    const BackgroundImageLayer& layer) {
  const std::string markup = extract_svg_markup_from_source(layer.image_url);
  if (markup.empty()) {
    return {};
  }

  const float width = parse_svg_length(extract_svg_attribute(markup, "width"));
  const float height = parse_svg_length(extract_svg_attribute(markup, "height"));
  if (width > 0.0f && height > 0.0f) {
    return {true, width / height};
  }

  std::string view_box = extract_svg_attribute(markup, "viewBox");
  if (view_box.empty()) {
    view_box = extract_svg_attribute(markup, "viewbox");
  }
  if (view_box.empty()) {
    return {};
  }

  std::replace(view_box.begin(), view_box.end(), ',', ' ');
  const auto tokens = split_pseudo_tokens(view_box);
  if (tokens.size() != 4) {
    return {};
  }

  const float view_box_width = parse_svg_length(tokens[2]);
  const float view_box_height = parse_svg_length(tokens[3]);
  if (view_box_width <= 0.0f || view_box_height <= 0.0f) {
    return {};
  }
  return {true, view_box_width / view_box_height};
}

std::pair<float, float> resolve_cover_contain_background_size(
    bool cover, float width, float height,
    const BackgroundIntrinsicMetrics& metrics) {
  if (width <= 0.0f || height <= 0.0f) {
    return {0.0f, 0.0f};
  }

  // CSS gradients have no intrinsic dimensions or proportions, and non-inline
  // background images do not expose decoded natural size here. Without an
  // intrinsic ratio, cover/contain falls back to the positioning area instead
  // of inventing external image dimensions.
  if (!metrics.has_aspect_ratio || metrics.aspect_ratio <= 0.0f) {
    return {width, height};
  }

  const float positioning_ratio = width / height;
  const bool fit_width =
      cover ? positioning_ratio > metrics.aspect_ratio
            : positioning_ratio < metrics.aspect_ratio;
  if (fit_width) {
    return {width, width / metrics.aspect_ratio};
  }
  return {height * metrics.aspect_ratio, height};
}

std::pair<float, float> resolve_background_size(
    const std::string& value, float width, float height,
    const BackgroundIntrinsicMetrics& intrinsic_metrics = {},
    const Element* elem = nullptr) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty() || trimmed == "auto") {
    return {width, height};
  }
  if (trimmed == "cover") {
    return resolve_cover_contain_background_size(true, width, height,
                                                 intrinsic_metrics);
  }
  if (trimmed == "contain") {
    return resolve_cover_contain_background_size(false, width, height,
                                                 intrinsic_metrics);
  }

  const auto tokens = split_background_tokens(trimmed);
  if (tokens.empty()) {
    return {width, height};
  }

  if (tokens.size() == 1) {
    if (tokens[0] == "auto") {
      return {width, height};
    }
    const float resolved_width = parse_pseudo_length(tokens[0], width, elem);
    if (resolved_width <= 0.0f) {
      return {width, height};
    }
    const float resolved_height =
        intrinsic_metrics.has_aspect_ratio && intrinsic_metrics.aspect_ratio > 0.0f
            ? resolved_width / intrinsic_metrics.aspect_ratio
            : height;
    return {resolved_width, resolved_height > 0.0f ? resolved_height : height};
  }

  const float resolved_width =
      tokens[0] == "auto" ? width : parse_pseudo_length(tokens[0], width, elem);
  const float resolved_height =
      tokens[1] == "auto" ? height : parse_pseudo_length(tokens[1], height, elem);
  if (tokens[0] == "auto" && tokens[1] != "auto" &&
      intrinsic_metrics.has_aspect_ratio && intrinsic_metrics.aspect_ratio > 0.0f &&
      resolved_height > 0.0f) {
    return {resolved_height * intrinsic_metrics.aspect_ratio, resolved_height};
  }
  if (tokens[0] != "auto" && tokens[1] == "auto" &&
      intrinsic_metrics.has_aspect_ratio && intrinsic_metrics.aspect_ratio > 0.0f &&
      resolved_width > 0.0f) {
    return {resolved_width, resolved_width / intrinsic_metrics.aspect_ratio};
  }
  return {
      resolved_width > 0.0f ? resolved_width : width,
      resolved_height > 0.0f ? resolved_height : height,
  };
}

enum class BackgroundRepeatAxis {
  Repeat,
  NoRepeat,
  Space,
  Round,
};

struct BackgroundRepeatMode {
  BackgroundRepeatAxis x = BackgroundRepeatAxis::Repeat;
  BackgroundRepeatAxis y = BackgroundRepeatAxis::Repeat;
};

struct BackgroundTileAxis {
  float start = 0.0f;
  float step = 0.0f;
  bool repeats = true;
};

struct BackgroundClipGeometry {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float radius = 0.0f;
};

struct ClipPathGeometry {
  bool enabled = false;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

std::array<float, 4> resolve_corner_radii(float width, float height,
                                          const float radii[4]) {
  std::array<float, 4> resolved{0.0f, 0.0f, 0.0f, 0.0f};
  const float max_radius = std::max(0.0f, std::min(width, height) * 0.5f);
  for (size_t i = 0; i < resolved.size(); ++i) {
    resolved[i] = std::clamp(radii[i], 0.0f, max_radius);
  }
  return resolved;
}

bool has_uniform_corner_radius(const std::array<float, 4>& radii) {
  return std::fabs(radii[0] - radii[1]) <= 0.001f &&
         std::fabs(radii[0] - radii[2]) <= 0.001f &&
         std::fabs(radii[0] - radii[3]) <= 0.001f;
}

std::string format_path_number(float value) {
  std::ostringstream out;
  out.setf(std::ios::fixed, std::ios::floatfield);
  out.precision(3);
  out << value;
  std::string text = out.str();
  while (text.size() > 1 && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  return text;
}

std::string rounded_rect_path(float x, float y, float width, float height,
                              const std::array<float, 4>& radii) {
  const float left = x;
  const float top = y;
  const float right = x + width;
  const float bottom = y + height;
  const float tl = radii[0];
  const float tr = radii[1];
  const float br = radii[2];
  const float bl = radii[3];

  std::ostringstream path;
  path << "M " << format_path_number(left + tl) << " " << format_path_number(top)
       << " H " << format_path_number(right - tr);
  if (tr > 0.0f) {
    path << " A " << format_path_number(tr) << " " << format_path_number(tr)
         << " 0 0 1 " << format_path_number(right) << " "
         << format_path_number(top + tr);
  } else {
    path << " L " << format_path_number(right) << " " << format_path_number(top);
  }
  path << " V " << format_path_number(bottom - br);
  if (br > 0.0f) {
    path << " A " << format_path_number(br) << " " << format_path_number(br)
         << " 0 0 1 " << format_path_number(right - br) << " "
         << format_path_number(bottom);
  } else {
    path << " L " << format_path_number(right) << " "
         << format_path_number(bottom);
  }
  path << " H " << format_path_number(left + bl);
  if (bl > 0.0f) {
    path << " A " << format_path_number(bl) << " " << format_path_number(bl)
         << " 0 0 1 " << format_path_number(left) << " "
         << format_path_number(bottom - bl);
  } else {
    path << " L " << format_path_number(left) << " "
         << format_path_number(bottom);
  }
  path << " V " << format_path_number(top + tl);
  if (tl > 0.0f) {
    path << " A " << format_path_number(tl) << " " << format_path_number(tl)
         << " 0 0 1 " << format_path_number(left + tl) << " "
         << format_path_number(top);
  } else {
    path << " L " << format_path_number(left) << " " << format_path_number(top);
  }
  path << " Z";
  return path.str();
}

BackgroundRepeatMode parse_background_repeat(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty() || trimmed == "repeat") {
    return {};
  }
  if (trimmed == "no-repeat") {
    return {BackgroundRepeatAxis::NoRepeat, BackgroundRepeatAxis::NoRepeat};
  }
  if (trimmed == "repeat-x") {
    return {BackgroundRepeatAxis::Repeat, BackgroundRepeatAxis::NoRepeat};
  }
  if (trimmed == "repeat-y") {
    return {BackgroundRepeatAxis::NoRepeat, BackgroundRepeatAxis::Repeat};
  }

  const auto parse_axis = [](const std::string& token) {
    if (token == "space") {
      return BackgroundRepeatAxis::Space;
    }
    if (token == "round") {
      return BackgroundRepeatAxis::Round;
    }
    if (token == "no-repeat") {
      return BackgroundRepeatAxis::NoRepeat;
    }
    return BackgroundRepeatAxis::Repeat;
  };

  BackgroundRepeatMode mode{BackgroundRepeatAxis::NoRepeat,
                            BackgroundRepeatAxis::NoRepeat};
  const auto tokens = split_background_tokens(trimmed);
  if (!tokens.empty()) {
    mode.x = parse_axis(tokens[0]);
  }
  if (tokens.size() > 1) {
    mode.y = parse_axis(tokens[1]);
  } else {
    mode.y = mode.x;
  }
  return mode;
}

BackgroundTileAxis resolve_background_tile_axis(BackgroundRepeatAxis mode,
                                                float origin_start,
                                                float origin_size,
                                                float clip_start,
                                                float& tile_size,
                                                float positioned_offset) {
  tile_size = std::max(tile_size, 0.001f);
  if (mode == BackgroundRepeatAxis::Round) {
    const int count = std::max(
        1, static_cast<int>(std::round(origin_size / tile_size)));
    tile_size = origin_size / static_cast<float>(count);
    return {origin_start, tile_size, count > 1};
  }
  if (mode == BackgroundRepeatAxis::Space) {
    const int count = static_cast<int>(std::floor(origin_size / tile_size));
    if (count >= 2) {
      const float gap =
          (origin_size - tile_size * static_cast<float>(count)) /
          static_cast<float>(count - 1);
      return {origin_start, tile_size + gap, true};
    }
    return {origin_start + positioned_offset, tile_size, false};
  }
  if (mode == BackgroundRepeatAxis::NoRepeat) {
    return {origin_start + positioned_offset, tile_size, false};
  }

  float start = origin_start + positioned_offset;
  while (start > clip_start) {
    start -= tile_size;
  }
  while (start + tile_size <= clip_start) {
    start += tile_size;
  }
  return {start, tile_size, true};
}

BackgroundClip parse_background_box_value(const std::string& value,
                                          BackgroundClip fallback) {
  const std::string lowered = to_lower_copy(trim_copy(value));
  if (lowered == "padding-box") {
    return BackgroundClip::PaddingBox;
  }
  if (lowered == "content-box") {
    return BackgroundClip::ContentBox;
  }
  if (lowered == "border-box") {
    return BackgroundClip::BorderBox;
  }
  return fallback;
}

BackgroundClipGeometry resolve_background_box_geometry(const ComputedStyle* style,
                                                       float width,
                                                       float height,
                                                       float radius,
                                                       BackgroundClip box) {
  BackgroundClipGeometry geometry{0.0f, 0.0f, width, height, radius};
  if (!style) {
    return geometry;
  }

  float inset_top = 0.0f;
  float inset_right = 0.0f;
  float inset_bottom = 0.0f;
  float inset_left = 0.0f;

  if (box == BackgroundClip::PaddingBox) {
    inset_top = style->border_width[0];
    inset_right = style->border_width[1];
    inset_bottom = style->border_width[2];
    inset_left = style->border_width[3];
  } else if (box == BackgroundClip::ContentBox) {
    inset_top = style->border_width[0] + style->padding[0];
    inset_right = style->border_width[1] + style->padding[1];
    inset_bottom = style->border_width[2] + style->padding[2];
    inset_left = style->border_width[3] + style->padding[3];
  }

  geometry.x = inset_left;
  geometry.y = inset_top;
  geometry.width = std::max(0.0f, width - inset_left - inset_right);
  geometry.height = std::max(0.0f, height - inset_top - inset_bottom);
  geometry.radius = std::max(0.0f, radius - std::max({inset_top, inset_right,
                                                       inset_bottom, inset_left}));
  return geometry;
}

BackgroundClipGeometry resolve_background_clip_geometry(const ComputedStyle* style,
                                                        float width,
                                                        float height,
                                                        float radius) {
  BackgroundClip clip = style ? style->background_clip : BackgroundClip::BorderBox;
  if (style) {
    clip = parse_background_box_value(
        style->get_variable(Symbol("--background-clip"), ""), clip);
  }
  return resolve_background_box_geometry(style, width, height, radius, clip);
}

BackgroundClipGeometry resolve_background_clip_geometry(
    const ComputedStyle* style, const std::string& layer_clip, float width,
    float height, float radius) {
  BackgroundClip clip = style ? style->background_clip : BackgroundClip::BorderBox;
  if (style) {
    clip = parse_background_box_value(
        style->get_variable(Symbol("--background-clip"), ""), clip);
  }
  clip = parse_background_box_value(layer_clip, clip);
  return resolve_background_box_geometry(style, width, height, radius, clip);
}

std::array<float, 4> resolve_inset_corner_radii(
    float width, float height, const BackgroundClipGeometry& clip,
    const std::array<float, 4>& outer_radii) {
  const float inset_left = clip.x;
  const float inset_top = clip.y;
  const float inset_right = std::max(0.0f, width - clip.x - clip.width);
  const float inset_bottom = std::max(0.0f, height - clip.y - clip.height);

  std::array<float, 4> inset_radii{
      std::max(0.0f, outer_radii[0] - std::max(inset_top, inset_left)),
      std::max(0.0f, outer_radii[1] - std::max(inset_top, inset_right)),
      std::max(0.0f, outer_radii[2] - std::max(inset_bottom, inset_right)),
      std::max(0.0f, outer_radii[3] - std::max(inset_bottom, inset_left)),
  };
  return resolve_corner_radii(clip.width, clip.height, inset_radii.data());
}

BackgroundClipGeometry resolve_background_origin_geometry(
    const ComputedStyle* style, const std::string& layer_origin, float width,
    float height, float radius) {
  BackgroundClip origin =
      style ? style->background_origin : BackgroundClip::PaddingBox;
  if (style) {
    origin = parse_background_box_value(
        style->get_variable(background_origin_var(), ""), origin);
  }
  origin = parse_background_box_value(layer_origin, origin);
  return resolve_background_box_geometry(style, width, height, radius, origin);
}

ClipPathGeometry resolve_clip_path_geometry(const ComputedStyle* style,
                                            float width, float height,
                                            const Element* elem = nullptr) {
  ClipPathGeometry geometry;
  if (!style || width <= 0.0f || height <= 0.0f) {
    return geometry;
  }

  const std::string value =
      to_lower_copy(trim_copy(style->get_variable(Symbol("--clip-path"), "")));
  if (value.empty() || value == "none") {
    return geometry;
  }

  static const std::string prefix = "inset(";
  if (value.rfind(prefix, 0) != 0 || value.back() != ')') {
    return geometry;
  }

  std::string inner = trim_copy(
      value.substr(prefix.size(), value.size() - prefix.size() - 1));
  auto tokens = split_pseudo_tokens(inner);
  const auto round_it = std::find(tokens.begin(), tokens.end(), "round");
  if (round_it != tokens.end()) {
    tokens.erase(round_it, tokens.end());
  }
  if (tokens.empty() || tokens.size() > 4) {
    return geometry;
  }

  inner.clear();
  for (const auto& token : tokens) {
    if (!inner.empty()) {
      inner.push_back(' ');
    }
    inner += token;
  }

  const auto insets = parse_pseudo_box_values(inner, width, height, elem);
  if (std::isnan(insets[0]) || std::isnan(insets[1]) ||
      std::isnan(insets[2]) || std::isnan(insets[3])) {
    return geometry;
  }

  geometry.enabled = true;
  geometry.x = insets[3];
  geometry.y = insets[0];
  geometry.width = std::max(0.0f, width - insets[1] - insets[3]);
  geometry.height = std::max(0.0f, height - insets[0] - insets[2]);
  return geometry;
}

float resolve_blur_radius(const ComputedStyle* style, Symbol variable_name,
                         float width, float height,
                         const Element* elem = nullptr) {
  if (!style) {
    return 0.0f;
  }

  const std::string raw = trim_copy(style->get_variable(variable_name, ""));
  if (raw.empty()) {
    return 0.0f;
  }

  return std::max(
      0.0f, parse_pseudo_length(raw, std::max(width, height), elem));
}

bool resolve_filter_drop_shadow(const ComputedStyle* style, float width,
                                float height, BoxShadow& shadow,
                                const Element* elem = nullptr) {
  if (!style) {
    return false;
  }

  const std::string raw_offset_x =
      trim_copy(style->get_variable(filter_drop_shadow_offset_x_var(), ""));
  const std::string raw_offset_y =
      trim_copy(style->get_variable(filter_drop_shadow_offset_y_var(), ""));
  if (raw_offset_x.empty() || raw_offset_y.empty()) {
    return false;
  }

  shadow = BoxShadow{};
  shadow.offset_x = parse_pseudo_length(raw_offset_x, width, elem);
  shadow.offset_y = parse_pseudo_length(raw_offset_y, height, elem);
  if (std::isnan(shadow.offset_x) || std::isnan(shadow.offset_y)) {
    return false;
  }

  shadow.blur_radius =
      resolve_blur_radius(style, filter_drop_shadow_blur_var(), width, height, elem);
  shadow.color = style->get_variable_color(
      filter_drop_shadow_color_var(), Color{0.0f, 0.0f, 0.0f, 1.0f});
  shadow.inset = false;
  return true;
}

float resolve_filter_opacity(const ComputedStyle* style) {
  if (!style) {
    return 1.0f;
  }
  const std::string raw = trim_copy(style->get_variable(filter_opacity_var(), ""));
  if (raw.empty()) {
    return 1.0f;
  }
  return std::clamp(std::strtof(raw.c_str(), nullptr), 0.0f, 1.0f);
}

bool is_scroll_overflow(Overflow value) {
  return value == Overflow::Auto || value == Overflow::Scroll;
}

flex::Paint to_linear_gradient_paint(const LinearGradient& gradient) {
  if (gradient.stop_count < 2) {
    return Paint::none();
  }

  const float radians = (gradient.angle - 90.0f) * 3.14159265f / 180.0f;
  const float dx = std::cos(radians);
  const float dy = std::sin(radians);
  const float extent = 0.5f / std::max(std::fabs(dx), std::fabs(dy));

  flex::LinearGradient linear;
  linear.x1 = 0.5f - dx * extent;
  linear.y1 = 0.5f - dy * extent;
  linear.x2 = 0.5f + dx * extent;
  linear.y2 = 0.5f + dy * extent;
  for (int i = 0; i < gradient.stop_count; ++i) {
    linear.add_stop(gradient.stops[i].offset, gradient.stops[i].color);
  }
  return flex::Paint(linear);
}

flex::Paint to_linear_gradient_paint(const BackgroundImageLayer& layer) {
  return to_linear_gradient_paint(layer.gradient);
}

float resolve_radial_radius(const std::string& size_value, float cx, float cy,
                            float width, float height,
                            const Element* elem = nullptr) {
  const auto corner_distance = [&](float x, float y) {
    return std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
  };
  const std::string trimmed = trim_copy(size_value);
  if (trimmed.empty() || trimmed == "farthest-corner") {
    return std::max({corner_distance(0.0f, 0.0f), corner_distance(1.0f, 0.0f),
                     corner_distance(0.0f, 1.0f), corner_distance(1.0f, 1.0f)});
  }
  if (trimmed == "closest-corner") {
    return std::min({corner_distance(0.0f, 0.0f), corner_distance(1.0f, 0.0f),
                     corner_distance(0.0f, 1.0f), corner_distance(1.0f, 1.0f)});
  }
  if (trimmed == "closest-side") {
    return std::min({cx, 1.0f - cx, cy, 1.0f - cy});
  }
  if (trimmed == "farthest-side") {
    return std::max({cx, 1.0f - cx, cy, 1.0f - cy});
  }

  const auto tokens = split_background_tokens(trimmed);
  if (tokens.empty()) {
    return std::max({corner_distance(0.0f, 0.0f), corner_distance(1.0f, 0.0f),
                     corner_distance(0.0f, 1.0f), corner_distance(1.0f, 1.0f)});
  }

  const float max_dimension = std::max(width, height);
  if (tokens.size() == 1) {
    return max_dimension > 0.0f
               ? parse_pseudo_length(tokens[0], max_dimension, elem) / max_dimension
               : 0.5f;
  }

  const float rx =
      width > 0.0f ? parse_pseudo_length(tokens[0], width, elem) / width : 0.5f;
  const float ry =
      height > 0.0f ? parse_pseudo_length(tokens[1], height, elem) / height : 0.5f;
  return std::max(rx, ry);
}

flex::Paint to_radial_gradient_paint(const ComputedStyle* style, float width,
                                     float height,
                                     const Element* elem = nullptr) {
  if (!style || style->gradient.stop_count < 2) {
    return Paint::none();
  }

  const auto center = resolve_radial_center(
      style->get_variable(Symbol("__flex_background_radial_position"), "center"),
      width, height, elem);

  flex::RadialGradient radial;
  radial.cx = center.first;
  radial.cy = center.second;
  radial.fx = radial.cx;
  radial.fy = radial.cy;
  radial.radius = std::max(
      resolve_radial_radius(
          style->get_variable(Symbol("__flex_background_radial_size"),
                              "farthest-corner"),
          radial.cx, radial.cy, width, height, elem),
      0.001f);
  for (int i = 0; i < style->gradient.stop_count; ++i) {
    radial.add_stop(style->gradient.stops[i].offset, style->gradient.stops[i].color);
  }
  return flex::Paint(radial);
}

flex::Paint to_radial_gradient_paint(const BackgroundImageLayer& layer, float width,
                                     float height,
                                     const Element* elem = nullptr) {
  if (!layer.has_gradient || layer.gradient.stop_count < 2) {
    return Paint::none();
  }

  const auto center = resolve_radial_center(
      layer.radial_position.empty() ? "center" : layer.radial_position, width,
      height, elem);

  flex::RadialGradient radial;
  radial.cx = center.first;
  radial.cy = center.second;
  radial.fx = radial.cx;
  radial.fy = radial.cy;
  radial.radius = std::max(
      resolve_radial_radius(layer.radial_size.empty() ? "farthest-corner"
                                                      : layer.radial_size,
                            radial.cx, radial.cy, width, height, elem),
      0.001f);
  for (int i = 0; i < layer.gradient.stop_count; ++i) {
    radial.add_stop(layer.gradient.stops[i].offset, layer.gradient.stops[i].color);
  }
  return flex::Paint(radial);
}

flex::Paint to_background_gradient_paint(const ComputedStyle* style, float width,
                                         float height,
                                         const Element* elem = nullptr) {
  if (!style || style->gradient.stop_count < 2) {
    return Paint::none();
  }

  const std::string gradient_type = style->get_variable(
      Symbol("__flex_background_gradient_type"), "linear");
  if (gradient_type == "radial") {
    return to_radial_gradient_paint(style, width, height, elem);
  }
  return to_linear_gradient_paint(style->gradient);
}

flex::Paint to_background_gradient_paint(const BackgroundImageLayer& layer,
                                         float width, float height,
                                         const Element* elem = nullptr) {
  if (!layer.has_gradient || layer.gradient.stop_count < 2) {
    return Paint::none();
  }
  if (layer.gradient_type == "radial") {
    return to_radial_gradient_paint(layer, width, height, elem);
  }
  return to_linear_gradient_paint(layer);
}

void draw_background_gradient(RenderCommandList& commands,
                              const ComputedStyle* style, float width,
                              float height, float radius,
                              const Element* elem = nullptr) {
  if (!style || style->gradient.stop_count < 2 || width <= 0.0f || height <= 0.0f) {
    return;
  }

  const auto clip = resolve_background_clip_geometry(style, width, height, radius);
  if (clip.width <= 0.0f || clip.height <= 0.0f) {
    return;
  }
  const auto origin =
      resolve_background_origin_geometry(style, "", width, height, radius);

  const auto tile_size = resolve_background_size(
      style->get_variable(Symbol("__flex_background_size"), ""), origin.width,
      origin.height, BackgroundIntrinsicMetrics{}, elem);
  float tile_width = std::max(tile_size.first, 0.001f);
  float tile_height = std::max(tile_size.second, 0.001f);
  const auto repeat_mode = parse_background_repeat(
      style->get_variable(Symbol("__flex_background_repeat"), ""));
  const auto position = resolve_background_position(
      style->get_variable(Symbol("__flex_background_position"), ""),
      origin.width - tile_width, origin.height - tile_height, elem);
  const auto x_axis = resolve_background_tile_axis(
      repeat_mode.x, origin.x, origin.width, clip.x, tile_width, position.first);
  const auto y_axis = resolve_background_tile_axis(
      repeat_mode.y, origin.y, origin.height, clip.y, tile_height, position.second);

  commands.save();
  commands.clip_rect(clip.x, clip.y, clip.width, clip.height);

  const int max_tiles = 64;
  int tiles_y = 0;
  for (float y = y_axis.start; tiles_y < max_tiles;
       y += y_axis.repeats ? y_axis.step : height + 1.0f) {
    if (y >= clip.y + clip.height) {
      break;
    }
    int tiles_x = 0;
    for (float x = x_axis.start; tiles_x < max_tiles;
         x += x_axis.repeats ? x_axis.step : width + 1.0f) {
      if (x >= clip.x + clip.width) {
        break;
      }
      const bool fills_box = !x_axis.repeats && !y_axis.repeats &&
                             std::fabs(x - clip.x) <= 0.001f &&
                             std::fabs(y - clip.y) <= 0.001f &&
                             std::fabs(tile_width - clip.width) <= 0.001f &&
                             std::fabs(tile_height - clip.height) <= 0.001f;
      commands.draw_rect(
          x, y, tile_width, tile_height, fills_box ? clip.radius : 0.0f,
          to_background_gradient_paint(style, tile_width, tile_height, elem),
          Paint::none(), 0.0f);
      ++tiles_x;
      if (!x_axis.repeats) {
        break;
      }
    }
    ++tiles_y;
    if (!y_axis.repeats) {
      break;
    }
  }

  commands.restore();
}

struct PseudoStyleSymbols {
  Symbol display;
  Symbol content;
  Symbol background_color;
  Symbol background;
  Symbol color;
  Symbol border_color;
  Symbol border_width;
  Symbol opacity;
  Symbol width;
  Symbol height;
  Symbol left;
  Symbol right;
  Symbol top;
  Symbol bottom;
  Symbol inset;
  Symbol font_size;
  Symbol border_radius;
};

const PseudoStyleSymbols& before_pseudo_symbols() {
  static const PseudoStyleSymbols symbols{
      Symbol("--before-display"),
      Symbol("--before-content"),
      Symbol("--before-background-color"),
      Symbol("--before-background"),
      Symbol("--before-color"),
      Symbol("--before-border-color"),
      Symbol("--before-border-width"),
      Symbol("--before-opacity"),
      Symbol("--before-width"),
      Symbol("--before-height"),
      Symbol("--before-left"),
      Symbol("--before-right"),
      Symbol("--before-top"),
      Symbol("--before-bottom"),
      Symbol("--before-inset"),
      Symbol("--before-font-size"),
      Symbol("--before-border-radius")};
  return symbols;
}

const PseudoStyleSymbols& after_pseudo_symbols() {
  static const PseudoStyleSymbols symbols{
      Symbol("--after-display"),
      Symbol("--after-content"),
      Symbol("--after-background-color"),
      Symbol("--after-background"),
      Symbol("--after-color"),
      Symbol("--after-border-color"),
      Symbol("--after-border-width"),
      Symbol("--after-opacity"),
      Symbol("--after-width"),
      Symbol("--after-height"),
      Symbol("--after-left"),
      Symbol("--after-right"),
      Symbol("--after-top"),
      Symbol("--after-bottom"),
      Symbol("--after-inset"),
      Symbol("--after-font-size"),
      Symbol("--after-border-radius")};
  return symbols;
}

bool has_variable(const ComputedStyle* style, Symbol name) {
  return style && style->variables.find(name) != style->variables.end();
}

bool has_visible_pseudo_variables(const ComputedStyle* style,
                                  const PseudoStyleSymbols& symbols) {
  return has_variable(style, symbols.content) ||
         has_variable(style, symbols.background_color) ||
         has_variable(style, symbols.background) ||
         has_variable(style, symbols.border_width);
}

void draw_pseudo_element(RenderCommandList& commands, Element* elem,
                         const ComputedStyle* style,
                         const PseudoStyleSymbols& symbols) {
  if (!elem || !style) {
    return;
  }
  if (!has_visible_pseudo_variables(style, symbols)) {
    return;
  }

  const std::string display =
      trim_copy(style->get_variable(symbols.display, "inline"));
  if (display == "none") {
    return;
  }

  const std::string content = style->get_variable(symbols.content, "");
  Color bg_color = style->get_variable_color(symbols.background_color,
                                             Color{0.0f, 0.0f, 0.0f, 0.0f});
  if (bg_color.a <= 0.0f) {
    bg_color = style->get_variable_color(symbols.background, bg_color);
  }
  const Color text_color =
      style->get_variable_color(symbols.color, style->text_color);
  Color border_color = style->get_variable_color(symbols.border_color,
                                                 Color{0.0f, 0.0f, 0.0f, 0.0f});
  const float border_width = parse_pseudo_length(
      style->get_variable(symbols.border_width, ""), elem->width(), elem);
  const float opacity =
      parse_pseudo_opacity(style->get_variable(symbols.opacity, ""), 1.0f);

  float width = parse_pseudo_length(
      style->get_variable(symbols.width, ""), elem->width(), elem);
  float height = parse_pseudo_length(
      style->get_variable(symbols.height, ""), elem->height(), elem);
  const float left = parse_pseudo_length(
      style->get_variable(symbols.left, ""), elem->width(), elem);
  const float right = parse_pseudo_length(
      style->get_variable(symbols.right, ""), elem->width(), elem);
  const float top = parse_pseudo_length(
      style->get_variable(symbols.top, ""), elem->height(), elem);
  const float bottom = parse_pseudo_length(
      style->get_variable(symbols.bottom, ""), elem->height(), elem);
  const std::string inset_value = style->get_variable(symbols.inset, "");
  const auto inset = parse_pseudo_box_values(
      inset_value, elem->width(), elem->height(), elem);
  const std::string font_size_value = style->get_variable(symbols.font_size, "");
  const float parsed_font_size =
      parse_pseudo_length(font_size_value, style->font_size, elem);
  const float font_size =
      std::isnan(parsed_font_size) ? style->font_size : parsed_font_size;
  const std::string radius_value = style->get_variable(symbols.border_radius, "");
  const float parsed_radius =
      parse_pseudo_length(radius_value, std::min(elem->width(), elem->height()),
                          elem);
  const float radius = std::isnan(parsed_radius) ? 0.0f : parsed_radius;

  const float resolved_left = std::isnan(left) ? inset[3] : left;
  const float resolved_right = std::isnan(right) ? inset[1] : right;
  const float resolved_top = std::isnan(top) ? inset[0] : top;
  const float resolved_bottom = std::isnan(bottom) ? inset[2] : bottom;

  if (std::isnan(width) && !std::isnan(resolved_left) && !std::isnan(resolved_right)) {
    width = std::max(elem->width() - resolved_left - resolved_right, 0.0f);
  }
  if (std::isnan(width)) {
    ComputedStyle pseudo_text_style = *style;
    pseudo_text_style.font_size = font_size;
    width = content.empty() ? elem->width()
                            : std::max(approximate_text_width(&pseudo_text_style, content),
                                       font_size);
  }
  if (std::isnan(height) && !std::isnan(resolved_top) && !std::isnan(resolved_bottom)) {
    height = std::max(elem->height() - resolved_top - resolved_bottom, 0.0f);
  }
  if (std::isnan(height)) {
    height = content.empty() ? elem->height() : font_size * 1.2f;
  }

  float x = 0.0f;
  float y = 0.0f;
  if (!std::isnan(resolved_left)) {
    x = resolved_left;
  } else if (!std::isnan(resolved_right)) {
    x = elem->width() - resolved_right - width;
  }
  if (!std::isnan(resolved_top)) {
    y = resolved_top;
  } else if (!std::isnan(resolved_bottom)) {
    y = elem->height() - resolved_bottom - height;
  }

  bg_color.a *= opacity;
  border_color.a *= opacity;
  Color resolved_text_color = text_color;
  resolved_text_color.a *= opacity;

  if (bg_color.a > 0.0f) {
    commands.draw_rect(x, y, width, height, radius, Paint::solid(bg_color),
                       Paint::none(), 0.0f);
  }
  if (border_width > 0.0f && border_color.a > 0.0f) {
    commands.draw_rect(x, y, width, height, radius, Paint::none(),
                       Paint::solid(border_color), border_width);
  }
  if (!content.empty()) {
    ComputedStyle pseudo_text_style = *style;
    pseudo_text_style.font_size = font_size;
    pseudo_text_style.text_align = TextAlign::Left;
    const auto text_block =
        layout_text_block(&pseudo_text_style, content, x, y, width, height,
                          resolved_text_color, TextVerticalAlign::Top);
    emit_text_block(commands, text_block);
  }
}

void draw_background_gradient_layer(RenderCommandList& commands,
                                    const ComputedStyle* style,
                                    const BackgroundImageLayer& layer,
                                    float width, float height, float radius,
                                    const Element* elem = nullptr) {
  if (!style || !layer.has_gradient || layer.gradient.stop_count < 2 ||
      width <= 0.0f || height <= 0.0f) {
    return;
  }

  const auto clip =
      resolve_background_clip_geometry(style, layer.clip, width, height, radius);
  if (clip.width <= 0.0f || clip.height <= 0.0f) {
    return;
  }
  const auto origin = resolve_background_origin_geometry(
      style, layer.origin, width, height, radius);

  const auto tile_size =
      resolve_background_size(layer.size, origin.width, origin.height,
                              BackgroundIntrinsicMetrics{}, elem);
  float tile_width = std::max(tile_size.first, 0.001f);
  float tile_height = std::max(tile_size.second, 0.001f);
  const auto repeat_mode = parse_background_repeat(layer.repeat);
  const auto position = resolve_background_position(
      layer.position, origin.width - tile_width, origin.height - tile_height,
      elem);
  const auto x_axis = resolve_background_tile_axis(
      repeat_mode.x, origin.x, origin.width, clip.x, tile_width, position.first);
  const auto y_axis = resolve_background_tile_axis(
      repeat_mode.y, origin.y, origin.height, clip.y, tile_height, position.second);

  commands.save();
  commands.clip_rect(clip.x, clip.y, clip.width, clip.height);

  const int max_tiles = 64;
  int tiles_y = 0;
  for (float y = y_axis.start; tiles_y < max_tiles;
       y += y_axis.repeats ? y_axis.step : height + 1.0f) {
    if (y >= clip.y + clip.height) {
      break;
    }
    int tiles_x = 0;
    for (float x = x_axis.start; tiles_x < max_tiles;
         x += x_axis.repeats ? x_axis.step : width + 1.0f) {
      if (x >= clip.x + clip.width) {
        break;
      }

      const auto paint =
          to_background_gradient_paint(layer, tile_width, tile_height, elem);
      if (paint.type != Paint::Type::None) {
        commands.draw_rect(x, y, tile_width, tile_height, clip.radius, paint,
                           Paint::none(), 0.0f);
      }

      ++tiles_x;
      if (!x_axis.repeats) {
        break;
      }
    }

    ++tiles_y;
    if (!y_axis.repeats) {
      break;
    }
  }

  commands.restore();
}

void draw_background_image_layer(RenderCommandList& commands,
                                 const flex::RendererCapabilities& caps,
                                 const ComputedStyle* style,
                                 const BackgroundImageLayer& layer, float width,
                                 float height, float radius,
                                 const Element* elem = nullptr) {
  if (!style || !layer.has_image_url || layer.image_url.empty() || width <= 0.0f ||
      height <= 0.0f) {
    return;
  }

  const auto clip =
      resolve_background_clip_geometry(style, layer.clip, width, height, radius);
  if (clip.width <= 0.0f || clip.height <= 0.0f) {
    return;
  }
  const auto origin = resolve_background_origin_geometry(
      style, layer.origin, width, height, radius);

  const auto tile_size =
      resolve_background_size(layer.size, origin.width, origin.height,
                              resolve_background_image_intrinsic_metrics(layer),
                              elem);
  float tile_width = std::max(tile_size.first, 0.001f);
  float tile_height = std::max(tile_size.second, 0.001f);
  const auto repeat_mode = parse_background_repeat(layer.repeat);
  const auto position = resolve_background_position(
      layer.position, origin.width - tile_width, origin.height - tile_height,
      elem);
  const auto x_axis = resolve_background_tile_axis(
      repeat_mode.x, origin.x, origin.width, clip.x, tile_width, position.first);
  const auto y_axis = resolve_background_tile_axis(
      repeat_mode.y, origin.y, origin.height, clip.y, tile_height, position.second);

  commands.save();
  commands.clip_rect(clip.x, clip.y, clip.width, clip.height);

  const bool svg_source = is_svg_source(layer.image_url);
  const bool use_svg = svg_source && caps.svg_images;
  if ((svg_source && !use_svg) || (!svg_source && !caps.raster_images)) {
    commands.restore();
    return;
  }

  const int max_tiles = 64;
  int tiles_y = 0;
  for (float y = y_axis.start; tiles_y < max_tiles;
       y += y_axis.repeats ? y_axis.step : height + 1.0f) {
    if (y >= clip.y + clip.height) {
      break;
    }
    int tiles_x = 0;
    for (float x = x_axis.start; tiles_x < max_tiles;
         x += x_axis.repeats ? x_axis.step : width + 1.0f) {
      if (x >= clip.x + clip.width) {
        break;
      }

      if (use_svg) {
        commands.draw_svg(layer.image_url, x, y, tile_width, tile_height);
      } else {
        commands.draw_image(layer.image_url, x, y, tile_width, tile_height);
      }

      ++tiles_x;
      if (!x_axis.repeats) {
        break;
      }
    }

    ++tiles_y;
    if (!y_axis.repeats) {
      break;
    }
  }

  commands.restore();
}

} // namespace

RenderCommandList RenderManager::build_frame_commands(
    const RenderFrame& frame,
    const flex::RendererCapabilities& capabilities) {
  RenderCommandList commands = make_render_commands(capabilities);
  commands.reserve(512);
  commands.begin_frame(frame.viewport);
  commands.clear(frame.clear_color);
  if (frame.root) {
    const Bounds viewport{0.0f, 0.0f, frame.viewport.width,
                          frame.viewport.height};
    render_element(frame.root, flex::make_identity(), capabilities, viewport,
                   commands);
    render_overlays(frame.root, capabilities, commands);
  }
  commands.end_frame();
  return commands;
}

void RenderManager::render_frame(const RenderFrame& frame) {
  RenderProfile profile;
  const bool profile_enabled = render_profile_enabled();
  if (profile_enabled) {
    profile.start = std::chrono::high_resolution_clock::now();
    active_render_profile = &profile;
  } else {
    active_render_profile = nullptr;
  }

  const auto caps = renderer_->capabilities();
  RenderCommandList frame_start = make_render_commands(caps);
  frame_start.begin_frame(frame.viewport);
  frame_start.clear(frame.clear_color);

  RenderCommandList frame_commands = make_render_commands(caps);
  frame_commands.reserve(512);
  if (frame.root) {
    auto tree_start = profile_enabled
                          ? std::chrono::high_resolution_clock::now()
                          : std::chrono::high_resolution_clock::time_point{};
    const Bounds viewport{0.0f, 0.0f, frame.viewport.width,
                          frame.viewport.height};
    render_element(frame.root, flex::make_identity(), caps, viewport,
                   frame_commands);
    if (profile_enabled) {
      profile.tree_ms += elapsed_ms(tree_start);
    }

    auto overlay_start = profile_enabled
                             ? std::chrono::high_resolution_clock::now()
                             : std::chrono::high_resolution_clock::time_point{};
    render_overlays(frame.root, caps, frame_commands);
    if (profile_enabled) {
      profile.overlay_ms += elapsed_ms(overlay_start);
    }
  }

  RenderCommandList frame_end = make_render_commands(caps);
  frame_end.end_frame();
  RenderCommandList frame_signature = make_render_commands(caps);
  frame_signature.reserve(frame_start.commands().size() +
                          frame_commands.commands().size() +
                          frame_end.commands().size());
  frame_signature.append(frame_start);
  frame_signature.append(frame_commands);
  frame_signature.append(frame_end);

  const bool force_repaint = renderer_->requires_surface_recreation();
  if (force_repaint && !retained_cache_.empty()) {
    retained_cache_.clear(*renderer_);
  }

  if (caps.retained_mode) {
    auto replay_start = profile_enabled
                            ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
    frame_signature.replay_retained(*renderer_, retained_cache_);
    if (profile_enabled) {
      profile.replay_ms += elapsed_ms(replay_start);
      profile.command_count +=
          static_cast<int>(frame_signature.commands().size());
    }
  } else {
    if (!retained_cache_.empty()) {
      retained_cache_.clear(*renderer_);
    }
    auto frame_start_time = profile_enabled
                                ? std::chrono::high_resolution_clock::now()
                                : std::chrono::high_resolution_clock::time_point{};
    replay_render_commands(*renderer_, frame_start);
    if (profile_enabled) {
      profile.frame_start_ms += elapsed_ms(frame_start_time);
    }

    replay_render_commands(*renderer_, frame_commands);

    auto frame_end_time = profile_enabled
                              ? std::chrono::high_resolution_clock::now()
                              : std::chrono::high_resolution_clock::time_point{};
    replay_render_commands(*renderer_, frame_end);
    if (profile_enabled) {
      profile.frame_end_ms += elapsed_ms(frame_end_time);
    }
  }

  if (profile_enabled && elapsed_ms(profile.start) > 32.0) {
    const double total_ms = elapsed_ms(profile.start);
    TLOG_INFOF("[RenderManager] Frame | total: {:.1f}ms | begin: {:.1f}ms | tree: {:.1f}ms | overlays: {:.1f}ms | end: {:.1f}ms | replay({}): {:.1f}ms | element-self({}): {:.1f}ms | effects: {:.1f}ms | bg: {:.1f}ms | stroke: {:.1f}ms | content: {:.1f}ms | pseudo: {:.1f}ms | widgets({}): {:.1f}ms | text({}): {:.1f}ms | sort: {:.1f}ms",
              total_ms, profile.frame_start_ms, profile.tree_ms,
              profile.overlay_ms, profile.frame_end_ms, profile.command_count,
              profile.replay_ms, profile.elements, profile.element_ms,
              profile.effect_ms, profile.background_ms, profile.stroke_ms,
              profile.content_ms, profile.pseudo_ms, profile.widgets,
              profile.widget_ms, profile.text_blocks, profile.text_ms,
              profile.sort_ms);
    if (!profile.widget_detail.empty()) {
      std::vector<std::pair<std::string, std::pair<int, double>>> detail(
          profile.widget_detail.begin(), profile.widget_detail.end());
      std::sort(detail.begin(), detail.end(),
                [](const auto& lhs, const auto& rhs) {
                  return lhs.second.second > rhs.second.second;
                });
      std::ostringstream out;
      const size_t limit = std::min<size_t>(detail.size(), 6);
      for (size_t i = 0; i < limit; ++i) {
        if (i > 0) {
          out << " | ";
        }
        out << detail[i].first << "(" << detail[i].second.first
            << "): " << detail[i].second.second << "ms";
      }
      TLOG_INFOF("[RenderManager]   widget detail | {}", out.str());
    }
  }
  active_render_profile = nullptr;
}

void RenderManager::render_tree(Element* root) {
  if (!root || !renderer_) return;
  const auto caps = renderer_->capabilities();
  const Bounds viewport = renderer_->viewport();
  RenderCommandList frame_commands = make_render_commands(caps);
  frame_commands.reserve(512);
  render_element(root, flex::make_identity(), caps, viewport, frame_commands);
  replay_render_commands(*renderer_, frame_commands);
}

void RenderManager::render_element(Element* elem,
                                   const Transform& parent_transform,
                                   const flex::RendererCapabilities& caps,
                                   const Bounds& viewport,
                                   RenderCommandList& frame_commands) {
  auto* style = elem->computed_style;
  if (!style || !elem->is_visible()) return;
  Widget* part_widget = nullptr;
  Element* part_host = nullptr;
  std::string_view part_name;
  if (elem->is_widget_owned()) {
    for (auto* ancestor = elem->parent_elem(); ancestor;
         ancestor = ancestor->parent_elem()) {
      if (ancestor->widget) {
        part_widget = ancestor->widget;
        part_host = ancestor;
        break;
      }
    }
    if (const auto* part = elem->attribute("part")) {
      part_name = *part;
    }
  }
  auto element_start =
      active_render_profile ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
  double child_elapsed_ms = 0.0;
  if (active_render_profile) {
    active_render_profile->elements++;
  }

  const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
  float transform_x = style->transform_x;
  float transform_y = style->transform_y;
  float transform_scale = style->transform_scale;
  float transform_scale_x = style->transform_scale_x;
  float transform_scale_y = style->transform_scale_y;
  float transform_rotate = style->transform_rotate;
  const float origin_x = style->transform_origin_x_percent
                             ? elem->width() * style->transform_origin_x
                             : style->transform_origin_x;
  const float origin_y = style->transform_origin_y_percent
                             ? elem->height() * style->transform_origin_y
                             : style->transform_origin_y;
  float opacity = style->opacity;
  Color border_color = style->border_color;
  bool border_color_effect_active = false;
  float outline_width = style->outline_width;
  float outline_offset = style->outline_offset;
  Color outline_color = style->outline_color;
  float ring_width = style->ring_width;
  float ring_offset = style->ring_offset;
  Color ring_color = style->ring_color;
  Color ring_offset_color = style->ring_offset_color;
  BoxShadow primary_shadow = style->shadow;
  if (transform_scale_x == 1.0f && transform_scale_y == 1.0f &&
      transform_scale != 1.0f) {
    transform_scale_x = transform_scale;
    transform_scale_y = transform_scale;
  }
  if (elem->owner_box_) {
    auto effect_start =
        active_render_profile ? std::chrono::high_resolution_clock::now()
                              : std::chrono::high_resolution_clock::time_point{};
    auto& transitions = elem->owner_box_->transitions();
    auto& animations = elem->owner_box_->animations();
    const float current_time = elem->owner_box_->time();
    if (transitions.has_any_active() &&
        transitions.has_active(element_id, current_time)) {
      transform_x =
          transitions.get(element_id, "transform-x", transform_x, current_time);
      transform_y =
          transitions.get(element_id, "transform-y", transform_y, current_time);
      transform_scale = transitions.get(element_id, "transform-scale",
                                        transform_scale, current_time);
      transform_scale_x = transitions.get(element_id, "transform-scale-x",
                                          transform_scale_x, current_time);
      transform_scale_y = transitions.get(element_id, "transform-scale-y",
                                          transform_scale_y, current_time);
      transform_rotate = transitions.get(element_id, "transform-rotate",
                                         transform_rotate, current_time);
      if (const auto* opacity_property =
              detail::style_property_descriptor(
                  detail::StylePropertyId::Opacity)) {
        opacity = transitions.get_float(
            element_id, *opacity_property, opacity, current_time);
      }
      const Color before_border_color = border_color;
      border_color = get_transition_color(
          transitions, element_id, detail::StylePropertyId::BorderColor,
          border_color, current_time);
      border_color_effect_active =
          border_color_effect_active ||
          std::fabs(border_color.r - before_border_color.r) > 0.001f ||
          std::fabs(border_color.g - before_border_color.g) > 0.001f ||
          std::fabs(border_color.b - before_border_color.b) > 0.001f ||
          std::fabs(border_color.a - before_border_color.a) > 0.001f;
      outline_width = transitions.get(element_id, "outline-width", outline_width,
                                      current_time);
      outline_offset =
          transitions.get(element_id, "outline-offset", outline_offset,
                          current_time);
      outline_color = get_transition_color(
          transitions, element_id, detail::StylePropertyId::OutlineColor,
          outline_color, current_time);
      ring_width = transitions.get(element_id, "ring-width", ring_width,
                                   current_time);
      ring_offset = transitions.get(element_id, "ring-offset", ring_offset,
                                    current_time);
      ring_color = get_transition_color(
          transitions, element_id, detail::StylePropertyId::RingColor,
          ring_color, current_time);
      ring_offset_color = get_transition_color(
          transitions, element_id, detail::StylePropertyId::RingOffsetColor,
          ring_offset_color, current_time);
      primary_shadow.offset_x =
          transitions.get(element_id, "box-shadow-offset-x",
                          primary_shadow.offset_x, current_time);
      primary_shadow.offset_y =
          transitions.get(element_id, "box-shadow-offset-y",
                          primary_shadow.offset_y, current_time);
      primary_shadow.blur_radius =
          transitions.get(element_id, "box-shadow-blur",
                          primary_shadow.blur_radius, current_time);
      primary_shadow.spread_radius =
          transitions.get(element_id, "box-shadow-spread",
                          primary_shadow.spread_radius, current_time);
      primary_shadow.color = get_transition_color(
          transitions, element_id, detail::StylePropertyId::BoxShadowColor,
          primary_shadow.color, current_time);
    }
    if (animations.has_any_effects() &&
        animations.has_effect(element_id, current_time)) {
      transform_x = get_animation_float(
          animations, element_id, detail::StylePropertyId::TransformX,
          transform_x, current_time);
      transform_y = get_animation_float(
          animations, element_id, detail::StylePropertyId::TransformY,
          transform_y, current_time);
      transform_scale = get_animation_float(
          animations, element_id, detail::StylePropertyId::TransformScale,
          transform_scale, current_time);
      transform_scale_x = get_animation_float(
          animations, element_id, detail::StylePropertyId::TransformScaleX,
          transform_scale_x, current_time);
      transform_scale_y = get_animation_float(
          animations, element_id, detail::StylePropertyId::TransformScaleY,
          transform_scale_y, current_time);
      transform_rotate = get_animation_float(
          animations, element_id, detail::StylePropertyId::TransformRotate,
          transform_rotate, current_time);
      if (const auto* opacity_property =
              detail::style_property_descriptor(
                  detail::StylePropertyId::Opacity)) {
        opacity = animations.get_float(
            element_id, *opacity_property, opacity, current_time);
      }
      const Color before_border_color = border_color;
      border_color.r =
          animations.get(element_id, "border-color-r", border_color.r, current_time);
      border_color.g =
          animations.get(element_id, "border-color-g", border_color.g, current_time);
      border_color.b =
          animations.get(element_id, "border-color-b", border_color.b, current_time);
      border_color.a =
          animations.get(element_id, "border-color-a", border_color.a, current_time);
      border_color_effect_active =
          border_color_effect_active ||
          std::fabs(border_color.r - before_border_color.r) > 0.001f ||
          std::fabs(border_color.g - before_border_color.g) > 0.001f ||
          std::fabs(border_color.b - before_border_color.b) > 0.001f ||
          std::fabs(border_color.a - before_border_color.a) > 0.001f;
      outline_width = get_animation_float(
          animations, element_id, detail::StylePropertyId::OutlineWidth,
          outline_width, current_time);
      outline_offset = get_animation_float(
          animations, element_id, detail::StylePropertyId::OutlineOffset,
          outline_offset, current_time);
      outline_color.r =
          animations.get(element_id, "outline-color-r", outline_color.r, current_time);
      outline_color.g =
          animations.get(element_id, "outline-color-g", outline_color.g, current_time);
      outline_color.b =
          animations.get(element_id, "outline-color-b", outline_color.b, current_time);
      outline_color.a =
          animations.get(element_id, "outline-color-a", outline_color.a, current_time);
      ring_width = get_animation_float(
          animations, element_id, detail::StylePropertyId::RingWidth,
          ring_width, current_time);
      ring_offset = get_animation_float(
          animations, element_id, detail::StylePropertyId::RingOffset,
          ring_offset, current_time);
      ring_color.r =
          animations.get(element_id, "ring-color-r", ring_color.r, current_time);
      ring_color.g =
          animations.get(element_id, "ring-color-g", ring_color.g, current_time);
      ring_color.b =
          animations.get(element_id, "ring-color-b", ring_color.b, current_time);
      ring_color.a =
          animations.get(element_id, "ring-color-a", ring_color.a, current_time);
      ring_offset_color.r = animations.get(
          element_id, "ring-offset-color-r", ring_offset_color.r, current_time);
      ring_offset_color.g = animations.get(
          element_id, "ring-offset-color-g", ring_offset_color.g, current_time);
      ring_offset_color.b = animations.get(
          element_id, "ring-offset-color-b", ring_offset_color.b, current_time);
      ring_offset_color.a = animations.get(
          element_id, "ring-offset-color-a", ring_offset_color.a, current_time);
      primary_shadow.offset_x = get_animation_float(
          animations, element_id, detail::StylePropertyId::BoxShadowOffsetX,
          primary_shadow.offset_x, current_time);
      primary_shadow.offset_y = get_animation_float(
          animations, element_id, detail::StylePropertyId::BoxShadowOffsetY,
          primary_shadow.offset_y, current_time);
      primary_shadow.blur_radius = get_animation_float(
          animations, element_id, detail::StylePropertyId::BoxShadowBlur,
          primary_shadow.blur_radius, current_time);
      primary_shadow.spread_radius = get_animation_float(
          animations, element_id, detail::StylePropertyId::BoxShadowSpread,
          primary_shadow.spread_radius, current_time);
      primary_shadow.color.r = animations.get(
          element_id, "box-shadow-color-r", primary_shadow.color.r, current_time);
      primary_shadow.color.g = animations.get(
          element_id, "box-shadow-color-g", primary_shadow.color.g, current_time);
      primary_shadow.color.b = animations.get(
          element_id, "box-shadow-color-b", primary_shadow.color.b, current_time);
      primary_shadow.color.a = animations.get(
          element_id, "box-shadow-color-a", primary_shadow.color.a, current_time);
    }
    if (transform_scale_x == 1.0f && transform_scale_y == 1.0f &&
        transform_scale != 1.0f) {
      transform_scale_x = transform_scale;
      transform_scale_y = transform_scale;
    }
    if (active_render_profile) {
      active_render_profile->effect_ms += elapsed_ms(effect_start);
    }
  }

  std::array<Color, 4> border_colors{};
  if (style->has_border_side_colors && !border_color_effect_active) {
    for (size_t i = 0; i < border_colors.size(); ++i) {
      border_colors[i] = style->border_colors[i];
    }
  } else {
    border_colors.fill(border_color);
  }

  using flex::operator*;

  Transform local_render_transform =
      flex::make_translation(elem->x() + transform_x, elem->y() + transform_y);
  if (caps.scaling &&
      (transform_scale_x != 1.0f || transform_scale_y != 1.0f)) {
    local_render_transform =
        local_render_transform * flex::make_translation(origin_x, origin_y) *
        flex::make_scale(transform_scale_x, transform_scale_y) *
        flex::make_translation(-origin_x, -origin_y);
  }
  if (caps.rotation && transform_rotate != 0.0f) {
    local_render_transform =
        local_render_transform * flex::make_translation(origin_x, origin_y) *
        flex::create_transform(0.0f, 0.0f, transform_rotate, 1.0f, 1.0f) *
        flex::make_translation(-origin_x, -origin_y);
  }
  if (style->has_transform_matrix) {
    local_render_transform =
        local_render_transform * flex::make_translation(origin_x, origin_y) *
        style->transform_matrix * flex::make_translation(-origin_x, -origin_y);
  }
  const Transform element_render_transform =
      parent_transform * local_render_transform;
  const Bounds render_bounds =
      Bounds{0.0f, 0.0f, elem->width(), elem->height()}
          .transformed(element_render_transform);
  if (render_bounds.x + render_bounds.width < viewport.x ||
      render_bounds.x > viewport.x + viewport.width ||
      render_bounds.y + render_bounds.height < viewport.y ||
      render_bounds.y > viewport.y + viewport.height) {
    if (active_render_profile) {
      active_render_profile->element_ms += elapsed_ms(element_start);
    }
    return;
  }

  frame_commands.save();

  // Transform
  if (style->has_transform_matrix) {
    frame_commands.set_transform(element_render_transform);
  } else {
    frame_commands.translate(elem->x() + transform_x,
                             elem->y() + transform_y);

    if (caps.scaling &&
        (transform_scale_x != 1.0f || transform_scale_y != 1.0f)) {
      frame_commands.translate(origin_x, origin_y);
      frame_commands.scale(transform_scale_x, transform_scale_y);
      frame_commands.translate(-origin_x, -origin_y);
    }

    if (caps.rotation && transform_rotate != 0.0f) {
      frame_commands.translate(origin_x, origin_y);
      frame_commands.rotate(transform_rotate);
      frame_commands.translate(-origin_x, -origin_y);
    }
  }
  frame_commands.set_global_alpha(opacity * resolve_filter_opacity(style));
  const auto clip_path =
      resolve_clip_path_geometry(style, elem->width(), elem->height(), elem);
  if (clip_path.enabled) {
    frame_commands.clip_rect(clip_path.x, clip_path.y, clip_path.width,
                             clip_path.height);
  }
  const bool widget_paints_host_box =
      (elem->widget && elem->widget->paints_host_box()) ||
      (part_widget && part_widget->paints_part_box(part_name));
  const float filter_blur_radius =
      resolve_blur_radius(style, Symbol("--filter-blur"), elem->width(),
                          elem->height(), elem);
  const float backdrop_blur_radius =
      resolve_blur_radius(style, Symbol("--backdrop-blur"), elem->width(),
                          elem->height(), elem);
  const bool use_filter_blur = caps.blur && filter_blur_radius > 0.0f;
  const bool use_backdrop_blur = caps.blur && backdrop_blur_radius > 0.0f;
  BoxShadow filter_drop_shadow;
  const bool has_filter_drop_shadow =
      resolve_filter_drop_shadow(style, elem->width(), elem->height(),
                                 filter_drop_shadow, elem);
  if (use_filter_blur) {
    frame_commands.set_blur(flex::BlurFilter(filter_blur_radius));
  }

  float radius = style->border_radius[0];
  const auto corner_radii =
      resolve_corner_radii(elem->width(), elem->height(), style->border_radius);
  const bool uniform_corner_radius = has_uniform_corner_radius(corner_radii);
  if (uniform_corner_radius) {
    radius = corner_radii[0];
  }
  float border_width = 0.0f;
  bool uniform_border = true;
  const float first_border_width = style->border_width[0];
  for (float side_width : style->border_width) {
    border_width = std::max(border_width, side_width);
    if (std::fabs(side_width - first_border_width) > 0.001f) {
      uniform_border = false;
    }
  }

  const auto draw_shadow = [&](const BoxShadow& shadow, bool inset_only) {
    if (shadow.inset != inset_only || shadow.color.a <= 0.0f) {
      return;
    }

    if (shadow.inset) {
      frame_commands.save();
      frame_commands.clip_rect(0, 0, elem->width(), elem->height());
    }

    flex::Shadow runtime_shadow;
    runtime_shadow.offset_x = shadow.offset_x;
    runtime_shadow.offset_y = shadow.offset_y;
    runtime_shadow.blur = shadow.blur_radius;
    runtime_shadow.spread = shadow.spread_radius;
    runtime_shadow.color = shadow.color;
    runtime_shadow.inset = shadow.inset;
    frame_commands.set_shadow(runtime_shadow);

    const bool use_shadow_blur = caps.blur && shadow.blur_radius > 0.0f;
    if (use_shadow_blur) {
      frame_commands.set_blur(flex::BlurFilter(shadow.blur_radius));
    }

    const float sx = shadow.offset_x - shadow.spread_radius;
    const float sy = shadow.offset_y - shadow.spread_radius;
    const float sw = elem->width() + shadow.spread_radius * 2;
    const float sh = elem->height() + shadow.spread_radius * 2;
    frame_commands.draw_rect(sx, sy, sw, sh, radius,
                             Paint::solid(shadow.color), Paint::none(), 0);

    if (use_shadow_blur) {
      if (use_filter_blur) {
        frame_commands.set_blur(flex::BlurFilter(filter_blur_radius));
      } else {
        frame_commands.clear_blur();
      }
    }
    frame_commands.clear_shadow();

    if (shadow.inset) {
      frame_commands.restore();
    }
  };

  // Outset shadows render behind the background.
  if (caps.shadow && (style->has_shadow || has_filter_drop_shadow)) {
    if (style->has_shadow) {
      if (!style->shadows.empty()) {
        for (size_t i = 0; i < style->shadows.size(); ++i) {
          draw_shadow(i == 0 ? primary_shadow : style->shadows[i], false);
        }
      } else {
        draw_shadow(primary_shadow, false);
      }
    }
    if (has_filter_drop_shadow) {
      draw_shadow(filter_drop_shadow, false);
    }
  }

  auto background_start =
      active_render_profile ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
  // Background
  if (use_backdrop_blur) {
    frame_commands.set_blur(flex::BlurFilter(backdrop_blur_radius));
  }
  Color bg_color = elem->is_widget_owned()
                       ? style->background_color
                       : style->get_variable_color("--bg", style->background_color);
  const auto bg_clip = resolve_background_clip_geometry(style, elem->width(),
                                                        elem->height(), radius);
  if (elem->owner_box_) {
    auto& transitions = elem->owner_box_->transitions();
    auto& animations = elem->owner_box_->animations();
    const float current_time = elem->owner_box_->time();
    if (transitions.has_any_active() &&
        transitions.has_active(element_id, current_time)) {
      bg_color = get_transition_color(
          transitions, element_id, detail::StylePropertyId::BackgroundColor,
          bg_color, current_time);
    }
    if (animations.has_any_effects() &&
        animations.has_effect(element_id, current_time)) {
      bg_color.r = animations.get(element_id, "background-color-r", bg_color.r,
                                  current_time);
      bg_color.g = animations.get(element_id, "background-color-g", bg_color.g,
                                  current_time);
      bg_color.b = animations.get(element_id, "background-color-b", bg_color.b,
                                  current_time);
      bg_color.a = animations.get(element_id, "background-color-a", bg_color.a,
                                  current_time);
    }
  }
  if (!widget_paints_host_box && bg_color.a > 0 && bg_clip.width > 0.0f &&
      bg_clip.height > 0.0f) {
    if (!uniform_corner_radius && caps.path_drawing) {
      const auto bg_clip_radii = resolve_inset_corner_radii(
          elem->width(), elem->height(), bg_clip, corner_radii);
      frame_commands.fill_path(
          rounded_rect_path(bg_clip.x, bg_clip.y, bg_clip.width,
                            bg_clip.height, bg_clip_radii),
          Paint::solid(bg_color));
    } else {
      frame_commands.draw_rect(bg_clip.x, bg_clip.y, bg_clip.width,
                               bg_clip.height, bg_clip.radius,
                               Paint::solid(bg_color), Paint::none(), 0);
    }
  }
  if (!widget_paints_host_box && !style->background_layers.empty()) {
    for (auto it = style->background_layers.rbegin();
         it != style->background_layers.rend(); ++it) {
      if (it->has_gradient) {
        draw_background_gradient_layer(frame_commands, style, *it, elem->width(),
                                       elem->height(), radius, elem);
      } else if (it->has_image_url) {
        draw_background_image_layer(frame_commands, caps, style, *it, elem->width(),
                                    elem->height(), radius, elem);
      }
    }
  } else if (!widget_paints_host_box && style->has_gradient &&
             style->gradient.stop_count >= 2) {
    draw_background_gradient(frame_commands, style, elem->width(), elem->height(),
                             radius, elem);
  }
  if (use_backdrop_blur) {
    if (use_filter_blur) {
      frame_commands.set_blur(flex::BlurFilter(filter_blur_radius));
    } else {
      frame_commands.clear_blur();
    }
  }

  // Inset shadows render over the background but stay clipped inside the box.
  if (caps.shadow && style->has_shadow) {
    if (!style->shadows.empty()) {
      for (size_t i = 0; i < style->shadows.size(); ++i) {
        draw_shadow(i == 0 ? primary_shadow : style->shadows[i], true);
      }
    } else {
      draw_shadow(primary_shadow, true);
    }
  }
  if (active_render_profile) {
    add_profile_time(active_render_profile->background_ms, background_start);
  }

  auto stroke_start =
      active_render_profile ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
  const auto draw_patterned_stroke_side = [&](RenderCommandList& commands,
                                              float x1, float y1, float x2,
                                              float y2, float width,
                                              BorderStyle style_kind,
                                              const Color& stroke_color) {
    if (width <= 0.0f || stroke_color.a <= 0.0f ||
        style_kind == BorderStyle::None ||
        style_kind == BorderStyle::Solid) {
      return;
    }

    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f) {
      return;
    }

    const float ux = dx / length;
    const float uy = dy / length;
    if (style_kind == BorderStyle::Double) {
      const float line_width = std::max(width / 3.0f, 1.0f);
      const float offset = std::max(width / 3.0f, line_width);
      const float nx = -uy;
      const float ny = ux;
      commands.draw_line(x1 + nx * offset, y1 + ny * offset,
                         x2 + nx * offset, y2 + ny * offset,
                         Paint::solid(stroke_color), line_width);
      commands.draw_line(x1 - nx * offset, y1 - ny * offset,
                         x2 - nx * offset, y2 - ny * offset,
                         Paint::solid(stroke_color), line_width);
      return;
    }
    const float dash_length =
        style_kind == BorderStyle::Dashed ? std::max(width * 3.0f, width)
                                          : std::max(width, 1.0f);
    const float gap_length =
        style_kind == BorderStyle::Dashed ? std::max(width * 2.0f, width)
                                          : std::max(width * 1.5f, width);

    for (float cursor = 0.0f; cursor < length; cursor += dash_length + gap_length) {
      const float seg_start = cursor;
      const float seg_end = std::min(length, cursor + dash_length);
      commands.draw_line(x1 + ux * seg_start, y1 + uy * seg_start,
                         x1 + ux * seg_end, y1 + uy * seg_end,
                         Paint::solid(stroke_color), width);
    }
  };

  const auto is_3d_border_style = [](BorderStyle style_kind) {
    return style_kind == BorderStyle::Groove || style_kind == BorderStyle::Ridge ||
           style_kind == BorderStyle::Inset || style_kind == BorderStyle::Outset;
  };
  const auto shade_border_color = [](const Color& color, bool light) {
    constexpr float amount = 0.35f;
    if (light) {
      return Color{color.r + (1.0f - color.r) * amount,
                   color.g + (1.0f - color.g) * amount,
                   color.b + (1.0f - color.b) * amount, color.a};
    }
    return Color{color.r * (1.0f - amount), color.g * (1.0f - amount),
                 color.b * (1.0f - amount), color.a};
  };
  const auto side_uses_light_edge = [](BorderStyle style_kind, int side,
                                       bool inner) {
    const bool top_or_left = side == 0 || side == 3;
    if (style_kind == BorderStyle::Inset) {
      return !top_or_left;
    }
    if (style_kind == BorderStyle::Outset) {
      return top_or_left;
    }
    if (style_kind == BorderStyle::Groove) {
      return inner ? top_or_left : !top_or_left;
    }
    if (style_kind == BorderStyle::Ridge) {
      return inner ? !top_or_left : top_or_left;
    }
    return true;
  };
  const auto draw_3d_border_side = [&](int side, float x, float y, float w,
                                       float h, float width,
                                       BorderStyle style_kind,
                                       const Color& base_color) {
    if (style_kind == BorderStyle::Groove || style_kind == BorderStyle::Ridge) {
      const float outer = std::max(width * 0.5f, 0.5f);
      const float inner = std::max(width - outer, 0.0f);
      const Color outer_color =
          shade_border_color(base_color, side_uses_light_edge(style_kind, side, false));
      const Color inner_color =
          shade_border_color(base_color, side_uses_light_edge(style_kind, side, true));
      if (side == 0) {
        frame_commands.draw_rect(x, y, w, outer, 0.0f,
                                 Paint::solid(outer_color), Paint::none(), 0.0f);
        frame_commands.draw_rect(x, y + outer, w, inner, 0.0f,
                                 Paint::solid(inner_color), Paint::none(), 0.0f);
      } else if (side == 1) {
        frame_commands.draw_rect(x + inner, y, outer, h, 0.0f,
                                 Paint::solid(outer_color), Paint::none(), 0.0f);
        frame_commands.draw_rect(x, y, inner, h, 0.0f,
                                 Paint::solid(inner_color), Paint::none(), 0.0f);
      } else if (side == 2) {
        frame_commands.draw_rect(x, y + inner, w, outer, 0.0f,
                                 Paint::solid(outer_color), Paint::none(), 0.0f);
        frame_commands.draw_rect(x, y, w, inner, 0.0f,
                                 Paint::solid(inner_color), Paint::none(), 0.0f);
      } else {
        frame_commands.draw_rect(x, y, outer, h, 0.0f,
                                 Paint::solid(outer_color), Paint::none(), 0.0f);
        frame_commands.draw_rect(x + outer, y, inner, h, 0.0f,
                                 Paint::solid(inner_color), Paint::none(), 0.0f);
      }
      return;
    }

    const Color side_color =
        shade_border_color(base_color, side_uses_light_edge(style_kind, side, false));
    frame_commands.draw_rect(x, y, w, h, 0.0f, Paint::solid(side_color),
                             Paint::none(), 0.0f);
  };

  const bool uniform_border_style =
      style->border_style[0] == style->border_style[1] &&
      style->border_style[0] == style->border_style[2] &&
      style->border_style[0] == style->border_style[3];
  const BorderStyle uniform_style = style->border_style[0];
  const auto same_color = [](const Color& lhs, const Color& rhs) {
    return std::fabs(lhs.r - rhs.r) <= 0.001f &&
           std::fabs(lhs.g - rhs.g) <= 0.001f &&
           std::fabs(lhs.b - rhs.b) <= 0.001f &&
           std::fabs(lhs.a - rhs.a) <= 0.001f;
  };
  const bool uniform_border_color = same_color(border_colors[0], border_colors[1]) &&
                                    same_color(border_colors[0], border_colors[2]) &&
                                    same_color(border_colors[0], border_colors[3]);

  if (!widget_paints_host_box && border_width > 0.0f &&
      border_colors[0].a > 0.0f && uniform_border && uniform_border_style &&
      uniform_border_color && uniform_style == BorderStyle::Solid) {
    if (!uniform_corner_radius && caps.path_drawing) {
      frame_commands.stroke_path(
          rounded_rect_path(0.0f, 0.0f, elem->width(), elem->height(),
                            corner_radii),
          Paint::solid(border_colors[0]), border_width);
    } else {
      frame_commands.draw_rect(0, 0, elem->width(), elem->height(), radius,
                               Paint::none(), Paint::solid(border_colors[0]),
                               border_width);
    }
  } else if (!widget_paints_host_box) {
    const float top = style->border_width[0];
    const float right = style->border_width[1];
    const float bottom = style->border_width[2];
    const float left = style->border_width[3];

    if (top > 0.0f && border_colors[0].a > 0.0f &&
        style->border_style[0] != BorderStyle::None) {
      if (is_3d_border_style(style->border_style[0])) {
        draw_3d_border_side(0, 0.0f, 0.0f, elem->width(), top, top,
                            style->border_style[0], border_colors[0]);
      } else if (style->border_style[0] == BorderStyle::Solid) {
        frame_commands.draw_rect(0, 0, elem->width(), top, 0.0f,
                                 Paint::solid(border_colors[0]), Paint::none(),
                                 0.0f);
      } else {
        draw_patterned_stroke_side(frame_commands, 0.0f, top * 0.5f,
                                   elem->width(), top * 0.5f, top,
                                   style->border_style[0], border_colors[0]);
      }
    }
    if (right > 0.0f && border_colors[1].a > 0.0f &&
        style->border_style[1] != BorderStyle::None) {
      if (is_3d_border_style(style->border_style[1])) {
        draw_3d_border_side(1, elem->width() - right, 0.0f, right,
                            elem->height(), right, style->border_style[1],
                            border_colors[1]);
      } else if (style->border_style[1] == BorderStyle::Solid) {
        frame_commands.draw_rect(elem->width() - right, 0, right,
                                 elem->height(), 0.0f,
                                 Paint::solid(border_colors[1]), Paint::none(),
                                 0.0f);
      } else {
        draw_patterned_stroke_side(frame_commands,
                                   elem->width() - right * 0.5f, 0.0f,
                                   elem->width() - right * 0.5f, elem->height(),
                                   right, style->border_style[1], border_colors[1]);
      }
    }
    if (bottom > 0.0f && border_colors[2].a > 0.0f &&
        style->border_style[2] != BorderStyle::None) {
      if (is_3d_border_style(style->border_style[2])) {
        draw_3d_border_side(2, 0.0f, elem->height() - bottom, elem->width(),
                            bottom, bottom, style->border_style[2],
                            border_colors[2]);
      } else if (style->border_style[2] == BorderStyle::Solid) {
        frame_commands.draw_rect(0, elem->height() - bottom, elem->width(),
                                 bottom, 0.0f, Paint::solid(border_colors[2]),
                                 Paint::none(), 0.0f);
      } else {
        draw_patterned_stroke_side(
            frame_commands, 0.0f, elem->height() - bottom * 0.5f, elem->width(),
            elem->height() - bottom * 0.5f, bottom, style->border_style[2],
            border_colors[2]);
      }
    }
    if (left > 0.0f && border_colors[3].a > 0.0f &&
        style->border_style[3] != BorderStyle::None) {
      if (is_3d_border_style(style->border_style[3])) {
        draw_3d_border_side(3, 0.0f, 0.0f, left, elem->height(), left,
                            style->border_style[3], border_colors[3]);
      } else if (style->border_style[3] == BorderStyle::Solid) {
        frame_commands.draw_rect(0, 0, left, elem->height(), 0.0f,
                                 Paint::solid(border_colors[3]), Paint::none(),
                                 0.0f);
      } else {
        draw_patterned_stroke_side(frame_commands, left * 0.5f, 0.0f,
                                   left * 0.5f, elem->height(), left,
                                   style->border_style[3], border_colors[3]);
      }
    }
  }

  const auto draw_outer_stroke = [&](float width, float offset,
                                     const Color& color,
                                     BorderStyle style_kind = BorderStyle::Solid) {
    if (width <= 0.0f || color.a <= 0.0f || style_kind == BorderStyle::None) {
      return;
    }
    const float expansion = offset + width * 0.5f;
    const float x = -expansion;
    const float y = -expansion;
    const float w = elem->width() + expansion * 2.0f;
    const float h = elem->height() + expansion * 2.0f;
    if (style_kind == BorderStyle::Solid) {
      frame_commands.draw_rect(x, y, w, h, radius + expansion, Paint::none(),
                               Paint::solid(color), width);
      return;
    }
    if (style_kind == BorderStyle::Double) {
      const float line_width = std::max(width / 3.0f, 1.0f);
      const float outer_expansion = offset + width - line_width * 0.5f;
      const float inner_expansion = offset + line_width * 0.5f;
      frame_commands.draw_rect(
          -outer_expansion, -outer_expansion,
          elem->width() + outer_expansion * 2.0f,
          elem->height() + outer_expansion * 2.0f,
          radius + outer_expansion, Paint::none(), Paint::solid(color),
          line_width);
      frame_commands.draw_rect(
          -inner_expansion, -inner_expansion,
          elem->width() + inner_expansion * 2.0f,
          elem->height() + inner_expansion * 2.0f,
          radius + inner_expansion, Paint::none(), Paint::solid(color),
          line_width);
      return;
    }
    if (is_3d_border_style(style_kind)) {
      draw_3d_border_side(0, x, y, w, width, width, style_kind, color);
      draw_3d_border_side(1, x + w - width, y, width, h, width, style_kind,
                          color);
      draw_3d_border_side(2, x, y + h - width, w, width, width, style_kind,
                          color);
      draw_3d_border_side(3, x, y, width, h, width, style_kind, color);
      return;
    }
    draw_patterned_stroke_side(frame_commands, x, y + width * 0.5f, x + w,
                               y + width * 0.5f, width, style_kind, color);
    draw_patterned_stroke_side(frame_commands, x + w - width * 0.5f, y,
                               x + w - width * 0.5f, y + h, width, style_kind,
                               color);
    draw_patterned_stroke_side(frame_commands, x, y + h - width * 0.5f, x + w,
                               y + h - width * 0.5f, width, style_kind, color);
    draw_patterned_stroke_side(frame_commands, x + width * 0.5f, y,
                               x + width * 0.5f, y + h, width, style_kind,
                               color);
  };
  // Ring and outline live outside the host surface. Widgets that paint their
  // own background/border still rely on the shared renderer for these CSS
  // focus affordances.
  draw_outer_stroke(ring_offset, 0.0f, ring_offset_color);
  draw_outer_stroke(ring_width, ring_offset, ring_color);
  draw_outer_stroke(outline_width, outline_offset, outline_color,
                    style->outline_style);
  if (active_render_profile) {
    add_profile_time(active_render_profile->stroke_ms, stroke_start);
  }

  auto content_start =
      active_render_profile ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
  const bool scroll_content = is_scroll_overflow(style->overflow_x) ||
                              is_scroll_overflow(style->overflow_y);
  const bool clip_overflow = style->overflow_x == Overflow::Hidden ||
                             style->overflow_y == Overflow::Hidden ||
                             scroll_content;
  if (clip_overflow && elem->width() > 0 && elem->height() > 0) {
    frame_commands.clip_rect(0, 0, elem->width(), elem->height());
  }

  if (scroll_content && (elem->scroll_x() > 0.0f || elem->scroll_y() > 0.0f)) {
    frame_commands.translate(-elem->scroll_x(), -elem->scroll_y());
  }
  Transform content_render_transform = element_render_transform;
  if (scroll_content && (elem->scroll_x() > 0.0f || elem->scroll_y() > 0.0f)) {
    content_render_transform =
        content_render_transform *
        flex::make_translation(-elem->scroll_x(), -elem->scroll_y());
  }
  if (active_render_profile) {
    add_profile_time(active_render_profile->content_ms, content_start);
  }

  if (has_visible_pseudo_variables(style, before_pseudo_symbols())) {
    auto before_pseudo_start =
        active_render_profile ? std::chrono::high_resolution_clock::now()
                              : std::chrono::high_resolution_clock::time_point{};
    frame_commands.push_transform_prefix(content_render_transform);
    draw_pseudo_element(frame_commands, elem, style,
                        before_pseudo_symbols());
    frame_commands.pop_transform_prefix();
    if (active_render_profile) {
      add_profile_time(active_render_profile->pseudo_ms, before_pseudo_start);
    }
  }

  // Widget content
  bool part_content_painted = false;
  if (elem->widget) {
    auto widget_start =
        active_render_profile ? std::chrono::high_resolution_clock::now()
                              : std::chrono::high_resolution_clock::time_point{};
    frame_commands.push_transform_prefix(content_render_transform);
    elem->widget->set_semantic_tree_rendering(true);
    elem->widget->emit_render_commands(*elem, frame_commands);
    elem->widget->set_semantic_tree_rendering(false);
    frame_commands.pop_transform_prefix();
    if (active_render_profile) {
      const double widget_elapsed = elapsed_ms(widget_start);
      active_render_profile->widgets++;
      active_render_profile->widget_ms += widget_elapsed;
      auto& detail =
          active_render_profile->widget_detail[elem->widget->type_name()];
      detail.first++;
      detail.second += widget_elapsed;
    }
  } else if (part_widget && part_host) {
    frame_commands.push_transform_prefix(content_render_transform);
    part_content_painted = part_widget->emit_part_render_commands(
        *part_host, *elem, part_name, frame_commands);
    frame_commands.pop_transform_prefix();
  }

  // Text
  if (!elem->widget && !part_content_painted && !elem->text().empty()) {
    auto text_start =
        active_render_profile ? std::chrono::high_resolution_clock::now()
                              : std::chrono::high_resolution_clock::time_point{};
    Color text_col = style->get_variable_color("--text-color", style->text_color);
    const float content_x = style->padding[3];
    const float content_y = style->padding[0];
    const float content_width =
        std::max(0.0f, elem->width() - style->padding[1] - style->padding[3]);
    const float content_height =
        std::max(0.0f, elem->height() - style->padding[0] - style->padding[2]);
    const auto text_block = layout_text_block(
        style, elem->text(), content_x, content_y, content_width, content_height,
        text_col, resolve_text_vertical_align(style, TextVerticalAlign::Middle));
    frame_commands.push_transform_prefix(content_render_transform);
    emit_text_block(frame_commands, text_block);
    frame_commands.pop_transform_prefix();
    if (active_render_profile) {
      active_render_profile->text_blocks++;
      active_render_profile->text_ms += elapsed_ms(text_start);
    }
  }

  // Children: low z-index first, high z-index last.
  auto sort_start =
      active_render_profile ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
  const auto& raw_children = elem->children();
  bool needs_sort = false;
  int previous_z = 0;
  bool have_previous_z = false;
  for (auto* node : raw_children) {
    auto* child = static_cast<Element*>(node);
    if (!child) {
      continue;
    }
    const int child_z = child->z_index();
    if (have_previous_z && child_z < previous_z) {
      needs_sort = true;
      break;
    }
    previous_z = child_z;
    have_previous_z = true;
  }
  std::vector<Element*> sorted_children;
  if (needs_sort) {
    sorted_children.reserve(raw_children.size());
    for (auto* node : raw_children) {
      if (auto* child = static_cast<Element*>(node)) {
        sorted_children.push_back(child);
      }
    }
    std::stable_sort(sorted_children.begin(), sorted_children.end(),
                     [](const Element* lhs, const Element* rhs) {
                       return lhs->z_index() < rhs->z_index();
                     });
  }
  if (active_render_profile) {
    active_render_profile->sort_ms += elapsed_ms(sort_start);
  }
  auto children_start =
      active_render_profile ? std::chrono::high_resolution_clock::now()
                            : std::chrono::high_resolution_clock::time_point{};
  if (needs_sort) {
    for (auto* child : sorted_children) {
      render_element(child, content_render_transform, caps, viewport,
                     frame_commands);
    }
  } else {
    for (auto* node : raw_children) {
      if (auto* child = static_cast<Element*>(node)) {
        render_element(child, content_render_transform, caps, viewport,
                       frame_commands);
      }
    }
  }
  if (active_render_profile) {
    child_elapsed_ms = elapsed_ms(children_start);
  }

  if (has_visible_pseudo_variables(style, after_pseudo_symbols())) {
    auto after_pseudo_start =
        active_render_profile ? std::chrono::high_resolution_clock::now()
                              : std::chrono::high_resolution_clock::time_point{};
    frame_commands.push_transform_prefix(content_render_transform);
    draw_pseudo_element(frame_commands, elem, style,
                        after_pseudo_symbols());
    frame_commands.pop_transform_prefix();
    if (active_render_profile) {
      add_profile_time(active_render_profile->pseudo_ms, after_pseudo_start);
    }
  }

  if (use_filter_blur) {
    frame_commands.clear_blur();
  }
  frame_commands.restore();

  elem->clear_dirty(flex::DirtyFlags::Visual);
  if (active_render_profile) {
    active_render_profile->element_ms +=
        elapsed_ms(element_start) - child_elapsed_ms;
  }
}

void RenderManager::render_overlays(Element* elem) {
  if (!elem || !renderer_ || !elem->is_visible()) return;
  const auto caps = renderer_->capabilities();
  RenderCommandList overlay_commands = make_render_commands(caps);
  render_overlays(elem, caps, overlay_commands);
  replay_render_commands(*renderer_, overlay_commands);
}

void RenderManager::render_overlays(
    Element* elem, const flex::RendererCapabilities& caps,
    RenderCommandList& frame_commands) {
  if (!elem || !elem->is_visible()) return;

  if (elem->widget && elem->widget->has_overlay()) {
    RenderCommandList overlay_commands =
        make_render_commands(caps);
    elem->widget->emit_overlay_commands(*elem, overlay_commands);
    frame_commands.append(overlay_commands);
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      render_overlays(child, caps, frame_commands);
    }
  }
}

} // namespace flexUI
