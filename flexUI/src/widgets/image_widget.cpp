/*
 * flexUI - ImageWidget Implementation
 */

#include <flexUI/widgets/image_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <vector>

namespace flexUI {

namespace {

bool is_svg_source(const std::string& src) {
  if (src.size() < 4) return false;
  const size_t suffix_pos = src.size() - 4;
  const char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 0])));
  const char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 1])));
  const char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 2])));
  const char c3 = static_cast<char>(std::tolower(static_cast<unsigned char>(src[suffix_pos + 3])));
  return c0 == '.' && c1 == 's' && c2 == 'v' && c3 == 'g';
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

std::vector<std::string> split_tokens(const std::string& value) {
  std::vector<std::string> tokens;
  std::string current;
  for (char ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      const std::string token = trim_copy(current);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      current.clear();
      continue;
    }
    current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }

  const std::string tail = trim_copy(current);
  if (!tail.empty()) {
    tokens.push_back(tail);
  }
  return tokens;
}

enum class ObjectPositionEdge {
  Start,
  Center,
  End,
};

struct ObjectPositionAxis {
  ObjectPositionEdge edge = ObjectPositionEdge::Center;
  float offset = 0.0f;
  bool has_offset = false;
  bool offset_is_percent = false;
};

bool is_horizontal_position_keyword(const std::string& token) {
  return token == "left" || token == "right";
}

bool is_vertical_position_keyword(const std::string& token) {
  return token == "top" || token == "bottom";
}

bool is_position_offset_token(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  char* end = nullptr;
  std::strtof(token.c_str(), &end);
  if (end == token.c_str()) {
    return false;
  }
  const std::string suffix = end ? std::string(end) : std::string();
  return suffix.empty() || suffix == "px" || suffix == "%";
}

std::optional<ObjectPositionAxis> parse_position_keyword(
    const std::string& token, bool horizontal) {
  if (token == "center") {
    return ObjectPositionAxis{};
  }
  if (horizontal) {
    if (token == "left") {
      return ObjectPositionAxis{ObjectPositionEdge::Start, 0.0f, false, false};
    }
    if (token == "right") {
      return ObjectPositionAxis{ObjectPositionEdge::End, 0.0f, false, false};
    }
  } else {
    if (token == "top") {
      return ObjectPositionAxis{ObjectPositionEdge::Start, 0.0f, false, false};
    }
    if (token == "bottom") {
      return ObjectPositionAxis{ObjectPositionEdge::End, 0.0f, false, false};
    }
  }
  return std::nullopt;
}

ObjectPositionAxis parse_position_offset(const std::string& token) {
  ObjectPositionAxis axis{ObjectPositionEdge::Start, 0.0f, true, false};
  axis.offset_is_percent = !token.empty() && token.back() == '%';
  axis.offset = std::strtof(token.c_str(), nullptr);
  return axis;
}

ObjectPositionAxis parse_position_axis(const std::string& token,
                                       bool horizontal) {
  if (const auto axis = parse_position_keyword(token, horizontal)) {
    return *axis;
  }
  if (is_position_offset_token(token)) {
    return parse_position_offset(token);
  }
  return ObjectPositionAxis{};
}

bool parse_position_pair(const std::vector<std::string>& tokens, size_t& index,
                         ObjectPositionAxis& x_axis,
                         ObjectPositionAxis& y_axis) {
  if (index >= tokens.size()) {
    return false;
  }

  const std::string& token = tokens[index];
  const bool horizontal = is_horizontal_position_keyword(token);
  const bool vertical = is_vertical_position_keyword(token);
  if (!horizontal && !vertical) {
    return false;
  }

  ObjectPositionAxis axis =
      *parse_position_keyword(token, horizontal /* horizontal axis */);
  if (index + 1 < tokens.size() &&
      is_position_offset_token(tokens[index + 1])) {
    const ObjectPositionAxis offset = parse_position_offset(tokens[index + 1]);
    axis.offset = offset.offset;
    axis.has_offset = true;
    axis.offset_is_percent = offset.offset_is_percent;
    index += 2;
  } else {
    ++index;
  }

  if (horizontal) {
    x_axis = axis;
  } else {
    y_axis = axis;
  }
  return true;
}

float resolve_object_position_axis(const ObjectPositionAxis& axis,
                                   float free_space) {
  const float offset =
      axis.offset_is_percent ? free_space * axis.offset / 100.0f : axis.offset;
  if (axis.edge == ObjectPositionEdge::Start) {
    return axis.has_offset ? offset : 0.0f;
  }
  if (axis.edge == ObjectPositionEdge::End) {
    return axis.has_offset ? free_space - offset : free_space;
  }
  return axis.has_offset ? free_space * 0.5f + offset : free_space * 0.5f;
}

std::pair<float, float> resolve_object_position(const std::string& value,
                                                float free_x, float free_y) {
  const auto tokens = split_tokens(value);
  if (tokens.empty()) {
    return {free_x * 0.5f, free_y * 0.5f};
  }

  ObjectPositionAxis x_axis;
  ObjectPositionAxis y_axis;
  if (tokens.size() == 1) {
    const auto& token = tokens[0];
    if (token == "left" || token == "right") {
      x_axis = parse_position_axis(token, true);
    } else if (token == "top" || token == "bottom") {
      y_axis = parse_position_axis(token, false);
    } else {
      x_axis = parse_position_axis(token, true);
    }
  } else if (tokens.size() == 2 &&
             is_horizontal_position_keyword(tokens[0]) &&
             is_position_offset_token(tokens[1])) {
    x_axis = *parse_position_keyword(tokens[0], true);
    const ObjectPositionAxis offset = parse_position_offset(tokens[1]);
    x_axis.offset = offset.offset;
    x_axis.has_offset = true;
    x_axis.offset_is_percent = offset.offset_is_percent;
  } else if (tokens.size() == 2 &&
             is_vertical_position_keyword(tokens[0]) &&
             is_position_offset_token(tokens[1])) {
    y_axis = *parse_position_keyword(tokens[0], false);
    const ObjectPositionAxis offset = parse_position_offset(tokens[1]);
    y_axis.offset = offset.offset;
    y_axis.has_offset = true;
    y_axis.offset_is_percent = offset.offset_is_percent;
  } else if (tokens.size() >= 3) {
    size_t index = 0;
    while (index < tokens.size()) {
      if (!parse_position_pair(tokens, index, x_axis, y_axis)) {
        break;
      }
    }
  } else {
    const bool first_vertical = is_vertical_position_keyword(tokens[0]);
    const bool second_horizontal = is_horizontal_position_keyword(tokens[1]);
    if (first_vertical && second_horizontal) {
      y_axis = parse_position_axis(tokens[0], false);
      x_axis = parse_position_axis(tokens[1], true);
    } else {
      x_axis = parse_position_axis(tokens[0], true);
      y_axis = parse_position_axis(tokens[1], false);
    }
  }

  return {
      resolve_object_position_axis(x_axis, free_x),
      resolve_object_position_axis(y_axis, free_y),
  };
}

} // namespace

ImageWidget::ImageWidget(const std::string& src) : src_(src) {
  // Image loading is deferred to RenderCommandList replay.
  // The backend renderer handles caching internally.
  if (!src_.empty()) {
    loaded_ = true;
  }
}

void ImageWidget::set_src(const std::string& src) {
  if (src_ != src) {
    src_ = src;
    loaded_ = !src.empty();
    error_ = false;
    natural_width_ = 0;
    natural_height_ = 0;
    dirty_ = true;
  }
}

void ImageWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  if (src_.empty()) return;

  auto* style = elem.computed_style;
  const auto& caps = commands.capabilities();

  const bool svg_source = is_svg_source(src_);
  if (svg_source) {
    if (caps.svg_images) {
      commands.draw_svg(src_, 0.0f, 0.0f, elem.width(), elem.height());
    }
  } else if (!caps.raster_images) {
    const Color border_color = style ? style->border_color : Color{0.4f, 0.4f, 0.4f, 1.0f};
    Color fill_color = style ? style->background_color : Color{0.9f, 0.9f, 0.9f, 1.0f};
    if (fill_color.a <= 0.0f) {
      fill_color = Color{0.9f, 0.9f, 0.9f, 1.0f};
    }
    commands.draw_rect(0.0f, 0.0f, elem.width(), elem.height(), 0.0f,
                       Paint::solid(fill_color), Paint::solid(border_color),
                       1.0f);
  } else {
    // Get object-fit mode
    std::string fit = "contain";
    std::string position = "center";
    if (style) {
      fit = style->get_variable("--object-fit", "contain");
      position = style->get_variable("--object-position", "center");
    }

    float elem_w = elem.width();
    float elem_h = elem.height();

    // For now, use element dimensions as image dimensions
    // RenderCommandList replay delegates actual loading and sizing to the backend.
    float draw_x = 0, draw_y = 0;
    float draw_w = elem_w, draw_h = elem_h;

    if (natural_width_ > 0 && natural_height_ > 0 &&
        (fit == "contain" || fit == "cover")) {
      const float image_ratio = natural_width_ / natural_height_;
      const float box_ratio = elem_h > 0.0f ? elem_w / elem_h : image_ratio;
      const bool fit_by_width =
          fit == "contain" ? image_ratio > box_ratio : image_ratio < box_ratio;
      if (fit_by_width) {
        draw_w = elem_w;
        draw_h = draw_w / image_ratio;
      } else {
        draw_h = elem_h;
        draw_w = draw_h * image_ratio;
      }
      const auto offsets =
          resolve_object_position(position, elem_w - draw_w, elem_h - draw_h);
      draw_x = offsets.first;
      draw_y = offsets.second;
    } else if (fit == "fill") {
      // Stretch to fill - use element dimensions directly
      draw_w = elem_w;
      draw_h = elem_h;
    } else if (fit == "cover") {
      // Scale to cover, may crop - for now just fill
      draw_w = elem_w;
      draw_h = elem_h;
    } else if (fit == "none") {
      // No scaling, center - use natural dimensions if known
      if (natural_width_ > 0 && natural_height_ > 0) {
        draw_w = natural_width_;
        draw_h = natural_height_;
        const auto offsets =
            resolve_object_position(position, elem_w - draw_w, elem_h - draw_h);
        draw_x = offsets.first;
        draw_y = offsets.second;
      }
    } else {
      // contain (default) - fit within bounds maintaining aspect ratio
      // Without knowing natural dimensions, just fill the element
      draw_w = elem_w;
      draw_h = elem_h;
    }

    commands.draw_image(src_, draw_x, draw_y, draw_w, draw_h);
  }

}

bool ImageWidget::handle_event(const Event& event, Element& elem) {
  return false;  // Images don't handle events by default
}

void ImageWidget::update(float delta_ms, Element& elem) {
  // No animation by default
}

} // namespace flexUI
