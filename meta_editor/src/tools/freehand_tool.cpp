/*
 * Meta Editor - Freehand Tool Implementation
 */ 

#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/tools/freehand_tool.h"
#include <cmath>
#include <iomanip>
#include <sstream>


namespace meta_editor {

bool FreehandTool::on_pointer_down(const flex::Vec2 &screen_pos, const flex::Vec2 &world_pos) {
  is_drawing_ = true;
  points_.clear();
  points_.push_back(world_pos);
  return true;
}

bool FreehandTool::on_pointer_move(const flex::Vec2 &screen_pos, const flex::Vec2 &world_pos) {
  if (!is_drawing_)
    return false;

  // Only add point if far enough from last point
  if (!points_.empty()) {
    auto &last = points_.back();
    float dx = world_pos.x - last.x;
    float dy = world_pos.y - last.y;
    if (dx * dx + dy * dy < min_distance_ * min_distance_) {
      return true;
    }
  }

  points_.push_back(world_pos);
  return true;
}

bool FreehandTool::on_pointer_up(const flex::Vec2 &screen_pos, const flex::Vec2 &world_pos) {
  if (!is_drawing_)
    return false;
  is_drawing_ = false;

  if (points_.size() < 2) {
    points_.clear();
    return true;
  }

  // Simplify path if smoothing enabled
  if (smoothing_ > 0) {
    simplify_points();
  }

  // Create path shape
  auto *layer = canvas_->content_root();
  auto *allocator = canvas_->instance()->object_allocator();

  auto *shape = flex::Shape::create(*allocator);
  shape->set_path(points_to_path());
  shape->set_position(0, 0); // Path contains absolute coordinates
  shape->set_stroke(stroke_color_, stroke_width_);
  layer->add_child(shape);

  selection_->clear_selection();
  selection_->select(shape);

  points_.clear();
  return true;
}

void FreehandTool::render_overlay(flex::Renderer &renderer) {
  if (!is_drawing_ || points_.size() < 2)
    return;

  flex::Paint stroke =
      flex::Paint::solid(flex::Color(stroke_color_.r, stroke_color_.g, stroke_color_.b, 0.7f));

  renderer.stroke_path(points_to_path(), stroke, stroke_width_);
}

std::string FreehandTool::points_to_path() const {
  if (points_.empty())
    return "";

  std::ostringstream ss;
  ss << std::fixed << std::setprecision(1);

  // Start with move to first point
  ss << "M " << points_[0].x << " " << points_[0].y;

  if (points_.size() == 2) {
    // Just a line
    ss << " L " << points_[1].x << " " << points_[1].y;
  } else if (smoothing_ > 0 && points_.size() >= 3) {
    // Use quadratic bezier curves for smoothing
    // First segment: line to midpoint
    float mx = (points_[0].x + points_[1].x) / 2;
    float my = (points_[0].y + points_[1].y) / 2;
    ss << " L " << mx << " " << my;

    // Middle segments: quadratic curves through midpoints
    for (size_t i = 1; i < points_.size() - 1; ++i) {
      float next_mx = (points_[i].x + points_[i + 1].x) / 2;
      float next_my = (points_[i].y + points_[i + 1].y) / 2;

      ss << " Q " << points_[i].x << " " << points_[i].y << " " << next_mx << " " << next_my;
    }

    // Last segment: line to end
    ss << " L " << points_.back().x << " " << points_.back().y;
  } else {
    // Raw polyline
    for (size_t i = 1; i < points_.size(); ++i) {
      ss << " L " << points_[i].x << " " << points_[i].y;
    }
  }

  return ss.str();
}

void FreehandTool::simplify_points() {
  if (points_.size() < 3)
    return;

  // Ramer-Douglas-Peucker simplification
  float epsilon = smoothing_ * 5.0f; // Tolerance based on smoothing level

  std::vector<bool> keep(points_.size(), false);
  keep[0] = true;
  keep[points_.size() - 1] = true;

  std::function<void(size_t, size_t)> simplify = [&](size_t start, size_t end) {
    if (end <= start + 1)
      return;

    float max_dist = 0;
    size_t max_idx = start;

    // Line from start to end
    float dx = points_[end].x - points_[start].x;
    float dy = points_[end].y - points_[start].y;
    float len_sq = dx * dx + dy * dy;

    for (size_t i = start + 1; i < end; ++i) {
      float dist;
      if (len_sq < 0.0001f) {
        // Start and end are same point
        float px = points_[i].x - points_[start].x;
        float py = points_[i].y - points_[start].y;
        dist = std::sqrt(px * px + py * py);
      } else {
        // Distance from point to line
        float t = ((points_[i].x - points_[start].x) * dx +
                   (points_[i].y - points_[start].y) * dy) /
                  len_sq;
        t = std::max(0.0f, std::min(1.0f, t));

        float proj_x = points_[start].x + t * dx;
        float proj_y = points_[start].y + t * dy;
        float px = points_[i].x - proj_x;
        float py = points_[i].y - proj_y;
        dist = std::sqrt(px * px + py * py);
      }

      if (dist > max_dist) {
        max_dist = dist;
        max_idx = i;
      }
    }

    if (max_dist > epsilon) {
      keep[max_idx] = true;
      simplify(start, max_idx);
      simplify(max_idx, end);
    }
  };

  simplify(0, points_.size() - 1);

  // Build simplified point list
  std::vector<flex::Vec2> simplified;
  for (size_t i = 0; i < points_.size(); ++i) {
    if (keep[i]) {
      simplified.push_back(points_[i]);
    }
  }

  points_ = std::move(simplified);
}

} // namespace meta_editor
