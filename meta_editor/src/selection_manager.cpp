/*
 * Meta Editor - Selection Manager Implementation
 */

#include "meta_editor/selection_manager.h"
#include "meta_editor/canvas.h"
#include <algorithm>

namespace meta_editor {

SelectionManager::SelectionManager(Canvas* canvas) : canvas_(canvas) {}

void SelectionManager::select(flex::Node* node) {
    if (!node) return;
    
    selected_nodes_.clear();
    selected_nodes_.push_back(node);
    notify_selection_change();
}

void SelectionManager::add_to_selection(flex::Node* node) {
    if (!node) return;
    
    // Check if already selected
    auto it = std::find(selected_nodes_.begin(), selected_nodes_.end(), node);
    if (it == selected_nodes_.end()) {
        selected_nodes_.push_back(node);
        notify_selection_change();
    }
}

void SelectionManager::remove_from_selection(flex::Node* node) {
    auto it = std::find(selected_nodes_.begin(), selected_nodes_.end(), node);
    if (it != selected_nodes_.end()) {
        selected_nodes_.erase(it);
        notify_selection_change();
    }
}

void SelectionManager::clear_selection() {
    if (!selected_nodes_.empty()) {
        selected_nodes_.clear();
        notify_selection_change();
    }
}

void SelectionManager::select_all() {
    // TODO: Implement select all
    selected_nodes_.clear();
    
    std::function<void(flex::Node*)> collect = [&](flex::Node* node) {
        if (!node) return;
        if (node->is_group()) {
            auto* group = static_cast<flex::Group*>(node);
            for (auto* child : group->children()) {
                if (!child->is_group()) {
                    selected_nodes_.push_back(child);
                }
                collect(child);
            }
        }
    };
    
    collect(canvas_->content_root());
    notify_selection_change();
}

bool SelectionManager::is_selected(flex::Node* node) const {
    return std::find(selected_nodes_.begin(), selected_nodes_.end(), node) != selected_nodes_.end();
}

flex::Node* SelectionManager::primary_selection() const {
    return selected_nodes_.empty() ? nullptr : selected_nodes_[0];
}

flex::Bounds SelectionManager::selection_bounds() const {
    if (selected_nodes_.empty()) {
        return flex::Bounds{0, 0, 0, 0};
    }

    flex::Bounds bounds = selected_nodes_[0]->world_bounds();
    for (size_t i = 1; i < selected_nodes_.size(); ++i) {
        auto nb = selected_nodes_[i]->world_bounds();
        float min_x = std::min(bounds.x, nb.x);
        float min_y = std::min(bounds.y, nb.y);
        float max_x = std::max(bounds.x + bounds.width, nb.x + nb.width);
        float max_y = std::max(bounds.y + bounds.height, nb.y + nb.height);
        bounds = flex::Bounds{min_x, min_y, max_x - min_x, max_y - min_y};
    }

    return bounds;
}

flex::Vec2 SelectionManager::selection_center() const {
    auto bounds = selection_bounds();
    return flex::Vec2(
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2
    );
}

void SelectionManager::render_selection_indicators(flex::Renderer& renderer) {
    if (selected_nodes_.empty()) return;

    auto bounds = selection_bounds();

    // Draw in world coordinates (camera transform is applied by caller)
    float x = bounds.x;
    float y = bounds.y;
    float w = bounds.width;
    float h = bounds.height;

    // Draw bounding box (stroke only, no fill)
    flex::Paint no_fill = flex::Paint::none();
    flex::Paint stroke = flex::Paint::solid(flex::Color(0.23f, 0.51f, 0.96f, 1.0f));
    renderer.draw_rect(x, y, w, h, 0, no_fill, stroke, 2.0f);

    // Draw resize handles (8 handles: 4 corners + 4 edges)
    float hs = 8.0f;  // handle size
    flex::Paint handle_fill = flex::Paint::solid(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
    flex::Paint handle_stroke = flex::Paint::solid(flex::Color(0.23f, 0.51f, 0.96f, 1.0f));

    // Corner handles
    renderer.draw_rect(x - hs/2, y - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);           // top-left
    renderer.draw_rect(x + w - hs/2, y - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);      // top-right
    renderer.draw_rect(x - hs/2, y + h - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);      // bottom-left
    renderer.draw_rect(x + w - hs/2, y + h - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);  // bottom-right

    // Edge handles
    renderer.draw_rect(x + w/2 - hs/2, y - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);    // top-center
    renderer.draw_rect(x + w/2 - hs/2, y + h - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);// bottom-center
    renderer.draw_rect(x - hs/2, y + h/2 - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);    // left-center
    renderer.draw_rect(x + w - hs/2, y + h/2 - hs/2, hs, hs, 0, handle_fill, handle_stroke, 1.0f);// right-center

    // Rotation handle (circle above top-center)
    float rotate_offset = 25.0f;
    float rotate_x = x + w / 2;
    float rotate_y = y - rotate_offset;
    renderer.draw_circle(rotate_x, rotate_y, 5.0f, handle_fill, handle_stroke, 1.5f);

    // Line connecting rotation handle to top-center
    char line_path[64];
    snprintf(line_path, sizeof(line_path), "M %.1f %.1f L %.1f %.1f", rotate_x, y, rotate_x, rotate_y + 5);
    renderer.stroke_path(line_path, handle_stroke, 1.0f);
}

HandleType SelectionManager::hit_test_handle(const flex::Vec2& screen_pos, float threshold) const {
    if (selected_nodes_.empty()) return HandleType::None;

    // Check rotation handle first (higher priority)
    flex::Vec2 rotate_pos = get_handle_position(HandleType::Rotate);
    float dx = screen_pos.x() - rotate_pos.x();
    float dy = screen_pos.y() - rotate_pos.y();
    if (dx * dx + dy * dy < threshold * threshold) {
        return HandleType::Rotate;
    }

    // Check resize handles
    for (int i = 0; i < 8; ++i) {
        HandleType handle = static_cast<HandleType>(i);
        flex::Vec2 hp = get_handle_position(handle);
        dx = screen_pos.x() - hp.x();
        dy = screen_pos.y() - hp.y();
        if (dx * dx + dy * dy < threshold * threshold) {
            return handle;
        }
    }
    return HandleType::None;
}

flex::Vec2 SelectionManager::get_handle_position(HandleType handle) const {
    auto bounds = selection_bounds();
    auto top_left = canvas_->world_to_screen(bounds.x, bounds.y);
    auto bottom_right = canvas_->world_to_screen(
        bounds.x + bounds.width,
        bounds.y + bounds.height
    );

    float x = top_left.x();
    float y = top_left.y();
    float w = bottom_right.x() - top_left.x();
    float h = bottom_right.y() - top_left.y();

    switch (handle) {
        case HandleType::TopLeft:      return flex::Vec2(x, y);
        case HandleType::TopCenter:    return flex::Vec2(x + w/2, y);
        case HandleType::TopRight:     return flex::Vec2(x + w, y);
        case HandleType::RightCenter:  return flex::Vec2(x + w, y + h/2);
        case HandleType::BottomRight:  return flex::Vec2(x + w, y + h);
        case HandleType::BottomCenter: return flex::Vec2(x + w/2, y + h);
        case HandleType::BottomLeft:   return flex::Vec2(x, y + h);
        case HandleType::LeftCenter:   return flex::Vec2(x, y + h/2);
        case HandleType::Rotate:       return flex::Vec2(x + w/2, y - 25.0f);
        default:                       return flex::Vec2(x, y);
    }
}

void SelectionManager::notify_selection_change() {
    if (selection_change_callback_) {
        selection_change_callback_();
    }
}

// Helper to check if node is a Shape
static flex::Shape* as_shape(flex::Node* node) {
    if (node && node->type() == flex::NodeType::Shape) {
        return static_cast<flex::Shape*>(node);
    }
    return nullptr;
}

// Helper to compare Paint objects
static bool paint_equals(const flex::Paint& a, const flex::Paint& b) {
    if (a.type != b.type) return false;
    if (a.type == flex::Paint::Type::None) return true;
    if (a.type == flex::Paint::Type::Solid) {
        return a.color.r == b.color.r && a.color.g == b.color.g &&
               a.color.b == b.color.b && a.color.a == b.color.a;
    }
    // For gradients, just compare by type (full comparison is complex)
    return true;
}

std::optional<flex::Paint> SelectionManager::get_common_fill() const {
    if (selected_nodes_.empty()) return std::nullopt;

    std::optional<flex::Paint> common;
    for (auto* node : selected_nodes_) {
        auto* shape = as_shape(node);
        if (!shape) continue;

        flex::Paint paint;
        if (shape->has_fill()) {
            auto f = shape->fill();
            if (f.type == flex::FillType::Solid) {
                paint = flex::Paint::solid(f.color);
            } else if (f.type == flex::FillType::LinearGradient) {
                paint = flex::Paint(f.linear_gradient);
            } else if (f.type == flex::FillType::RadialGradient) {
                paint = flex::Paint(f.radial_gradient);
            }
        } else {
            paint = flex::Paint::none();
        }

        if (!common.has_value()) {
            common = paint;
        } else if (!paint_equals(*common, paint)) {
            return std::nullopt;  // Mixed fills
        }
    }
    return common;
}

std::optional<flex::Paint> SelectionManager::get_common_stroke() const {
    if (selected_nodes_.empty()) return std::nullopt;

    std::optional<flex::Paint> common;
    for (auto* node : selected_nodes_) {
        auto* shape = as_shape(node);
        if (!shape) continue;

        flex::Paint paint;
        if (shape->has_stroke()) {
            auto s = shape->stroke();
            if (s.type == flex::StrokeType::Solid) {
                paint = flex::Paint::solid(s.color);
            } else if (s.type == flex::StrokeType::LinearGradient) {
                paint = flex::Paint(s.linear_gradient);
            } else if (s.type == flex::StrokeType::RadialGradient) {
                paint = flex::Paint(s.radial_gradient);
            }
        } else {
            paint = flex::Paint::none();
        }

        if (!common.has_value()) {
            common = paint;
        } else if (!paint_equals(*common, paint)) {
            return std::nullopt;
        }
    }
    return common;
}

std::optional<float> SelectionManager::get_common_stroke_width() const {
    if (selected_nodes_.empty()) return std::nullopt;

    std::optional<float> common;
    for (auto* node : selected_nodes_) {
        auto* shape = as_shape(node);
        if (!shape || !shape->has_stroke()) continue;

        float width = shape->stroke().width;
        if (!common.has_value()) {
            common = width;
        } else if (std::abs(*common - width) > 0.01f) {
            return std::nullopt;
        }
    }
    return common;
}

void SelectionManager::set_fill(const flex::Paint& paint) {
    for (auto* node : selected_nodes_) {
        auto* shape = as_shape(node);
        if (!shape) continue;

        switch (paint.type) {
            case flex::Paint::Type::None:
                shape->clear_fill();
                break;
            case flex::Paint::Type::Solid:
                shape->set_fill(paint.color);
                break;
            case flex::Paint::Type::Linear:
                shape->set_fill(paint.linear);
                break;
            case flex::Paint::Type::Radial:
                shape->set_fill(paint.radial);
                break;
        }
    }
}

void SelectionManager::set_stroke(const flex::Paint& paint, float width) {
    for (auto* node : selected_nodes_) {
        auto* shape = as_shape(node);
        if (!shape) continue;

        switch (paint.type) {
            case flex::Paint::Type::None:
                shape->clear_stroke();
                break;
            case flex::Paint::Type::Solid:
                shape->set_stroke(paint.color, width);
                break;
            case flex::Paint::Type::Linear:
                shape->set_stroke(paint.linear, width);
                break;
            case flex::Paint::Type::Radial:
                shape->set_stroke(paint.radial, width);
                break;
        }
    }
}

void SelectionManager::clear_fill() {
    for (auto* node : selected_nodes_) {
        if (auto* shape = as_shape(node)) {
            shape->clear_fill();
        }
    }
}

void SelectionManager::clear_stroke() {
    for (auto* node : selected_nodes_) {
        if (auto* shape = as_shape(node)) {
            shape->clear_stroke();
        }
    }
}

void SelectionManager::lock_selection() {
    for (auto* node : selected_nodes_) {
        locked_nodes_.insert(node);
    }
}

void SelectionManager::unlock_selection() {
    for (auto* node : selected_nodes_) {
        locked_nodes_.erase(node);
    }
}

bool SelectionManager::is_locked(flex::Node* node) const {
    return locked_nodes_.find(node) != locked_nodes_.end();
}

void SelectionManager::toggle_lock(flex::Node* node) {
    if (is_locked(node)) {
        locked_nodes_.erase(node);
    } else {
        locked_nodes_.insert(node);
    }
}

} // namespace meta_editor
