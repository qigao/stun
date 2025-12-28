/*
 * Meta Editor - Canvas Implementation
 */

#include "meta_editor/canvas.h"
#include <algorithm>
#include <cmath>

namespace meta_editor {

Canvas::Canvas(float width, float height) {
    instance_ = flex::Instance::create(width, height);
    scene_ = instance_->scene();
    
    // Create layer hierarchy
    // 1. Grid layer (background, locked)
    grid_layer_ = scene_->root()->add<flex::Group>();
    grid_layer_->set_id("grid_layer");
    locked_layers_[grid_layer_] = true;
    rebuild_grid();
    
    // 2. Content root (user layers)
    content_root_ = scene_->root()->add<flex::Group>();
    content_root_->set_id("content_root");
    
    // 3. UI overlay (screen space - selection handles, gizmos)
    ui_overlay_root_ = scene_->root()->add<flex::Group>();
    ui_overlay_root_->set_id("ui_overlay");
}

Canvas::~Canvas() = default;

void Canvas::set_size(float width, float height) {
    if (scene_) {
        scene_->set_size(width, height);
    }
}

float Canvas::width() const {
    return scene_ ? scene_->width() : 0.0f;
}

float Canvas::height() const {
    return scene_ ? scene_->height() : 0.0f;
}

// Layer Management
flex::Group* Canvas::create_layer(const std::string& name, flex::Group* parent) {
    flex::Group* parent_group = parent ? parent : content_root_;
    auto* group = parent_group->add<flex::Group>();
    group->set_id(name);
    locked_layers_[group] = false;
    notify_layer_change();
    return group;
}

bool Canvas::delete_layer(flex::Group* layer) {
    if (!layer || layer == content_root_ || layer == grid_layer_ || layer == ui_overlay_root_) {
        return false;
    }
    auto* parent = layer->parent();
    if (parent && parent->is_group()) {
        static_cast<flex::Group*>(parent)->remove_child(layer);
        locked_layers_.erase(layer);
        notify_layer_change();
        return true;
    }
    return false;
}

void Canvas::reorder_layer(flex::Group* layer, int new_index) {
    if (!layer) return;
    auto* parent = layer->parent();
    if (parent && parent->is_group()) {
        static_cast<flex::Group*>(parent)->insert_child(layer, (size_t)new_index);
        notify_layer_change();
    }
}

flex::Group* Canvas::get_layer_by_name(const std::string& name) {
    std::function<flex::Group*(flex::Node*)> search = [&](flex::Node* node) -> flex::Group* {
        if (node->id() == name && node->is_group()) {
            return static_cast<flex::Group*>(node);
        }
        if (node->is_group()) {
            auto* group = static_cast<flex::Group*>(node);
            for (auto* child : group->children()) {
                if (auto* found = search(child)) {
                    return found;
                }
            }
        }
        return nullptr;
    };
    return search(content_root_);
}

std::vector<flex::Group*> Canvas::get_all_layers() const {
    std::vector<flex::Group*> layers;
    std::function<void(flex::Node*)> collect = [&](flex::Node* node) {
        if (node->is_group()) {
            auto* group = static_cast<flex::Group*>(node);
            if (group != content_root_ && group != grid_layer_ && group != ui_overlay_root_) {
                layers.push_back(group);
            }
            for (auto* child : group->children()) {
                collect(child);
            }
        }
    };
    collect(content_root_);
    return layers;
}

// Layer Properties
void Canvas::set_layer_visible(flex::Group* layer, bool visible) {
    if (layer) {
        layer->set_visible(visible);
        notify_layer_change();
    }
}

void Canvas::set_layer_locked(flex::Group* layer, bool locked) {
    if (layer) {
        locked_layers_[layer] = locked;
        notify_layer_change();
    }
}

void Canvas::set_layer_opacity(flex::Group* layer, float opacity) {
    if (layer) {
        layer->set_opacity(opacity);
    }
}

bool Canvas::is_layer_locked(flex::Group* layer) const {
    if (!layer) return false;
    auto it = locked_layers_.find(layer);
    return it != locked_layers_.end() && it->second;
}

// Camera Control
void Canvas::pan(float dx, float dy) {
    camera_pan_x_ += dx;
    camera_pan_y_ += dy;
}

void Canvas::zoom(float delta) {
    camera_zoom_ *= delta;
    camera_zoom_ = std::clamp(camera_zoom_, min_zoom_, max_zoom_);
}

void Canvas::zoom_at(float screen_x, float screen_y, float delta) {
    // Get world position before zoom
    auto world_pos = screen_to_world(screen_x, screen_y);
    
    // Apply zoom
    zoom(delta);
    
    // Get world position after zoom (at same screen position)
    auto new_world_pos = screen_to_world(screen_x, screen_y);
    
    // Adjust pan to keep world position stationary
    auto offset = new_world_pos - world_pos;
    camera_pan_x_ -= offset.x() * camera_zoom_;
    camera_pan_y_ -= offset.y() * camera_zoom_;
}

void Canvas::reset_camera() {
    camera_pan_x_ = 0;
    camera_pan_y_ = 0;
    camera_zoom_ = 1.0f;
}

void Canvas::fit_to_view() {
    // TODO: Calculate bounds of all content and fit to view
    reset_camera();
}

// Coordinate Conversion
flex::Transform Canvas::camera_transform() const {
    return flex::make_translation(camera_pan_x_, camera_pan_y_) *
           flex::make_scale(camera_zoom_, camera_zoom_);
}

flex::Transform Canvas::inverse_camera_transform() const {
    return camera_transform().inverse();
}

flex::Vec2 Canvas::screen_to_world(float screen_x, float screen_y) const {
    return inverse_camera_transform() * flex::Vec2(screen_x, screen_y);
}

flex::Vec2 Canvas::world_to_screen(float world_x, float world_y) const {
    return camera_transform() * flex::Vec2(world_x, world_y);
}

flex::Vec2 Canvas::screen_to_world(const flex::Vec2& screen_pos) const {
    return screen_to_world(screen_pos.x(), screen_pos.y());
}

flex::Vec2 Canvas::world_to_screen(const flex::Vec2& world_pos) const {
    return world_to_screen(world_pos.x(), world_pos.y());
}

// Hit Testing
flex::Node* Canvas::hit_test(float screen_x, float screen_y) {
    auto world_pos = screen_to_world(screen_x, screen_y);
    return hit_test(world_pos);
}

flex::Node* Canvas::hit_test(const flex::Vec2& world_pos) {
    std::function<flex::Node*(flex::Node*, const flex::Vec2&)>
    hit_test_node = [&](flex::Node* node, const flex::Vec2& pos) -> flex::Node* {
        if (!node || !node->visible()) return nullptr;

        if (node->is_group()) {
            auto* group = static_cast<flex::Group*>(node);

            // Skip locked layers
            auto it = locked_layers_.find(group);
            if (it != locked_layers_.end() && it->second) {
                return nullptr;
            }

            // Test children in reverse order (top to bottom)
            auto& children = group->children();
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                if (auto* hit = hit_test_node(*it, pos)) {
                    return hit;
                }
            }
            // Groups themselves are not hit-testable
            return nullptr;
        }

        // Leaf node - test bounds in world space
        auto bounds = node->world_bounds();
        if (bounds.contains(pos.x(), pos.y())) {
            return node;
        }
        return nullptr;
    };

    return hit_test_node(content_root_, world_pos);
}

// Grid
void Canvas::set_grid_visible(bool visible) {
    grid_visible_ = visible;
    grid_layer_->set_visible(visible);
}

void Canvas::set_grid_size(float size) {
    if (size != grid_size_) {
        grid_size_ = size;
        rebuild_grid();
    }
}

void Canvas::set_snap_to_grid(bool enabled) {
    snap_to_grid_ = enabled;
}

flex::Vec2 Canvas::snap_to_grid(const flex::Vec2& pos) const {
    if (!snap_to_grid_) return pos;
    
    return flex::Vec2(
        std::round(pos.x() / grid_size_) * grid_size_,
        std::round(pos.y() / grid_size_) * grid_size_
    );
}

void Canvas::rebuild_grid() {
    grid_layer_->clear();

    // Build a single path containing all grid lines
    std::string path_data;
    const float extent = 1000.0f;

    // Vertical lines
    for (int i = -20; i <= 20; ++i) {
        float x = i * grid_size_;
        path_data += "M " + std::to_string(x) + " " + std::to_string(-extent) +
                     " L " + std::to_string(x) + " " + std::to_string(extent) + " ";
    }

    // Horizontal lines
    for (int i = -20; i <= 20; ++i) {
        float y = i * grid_size_;
        path_data += "M " + std::to_string(-extent) + " " + std::to_string(y) +
                     " L " + std::to_string(extent) + " " + std::to_string(y) + " ";
    }

    auto* grid = grid_layer_->add<flex::PathShape>();
    grid->set_path_data(path_data);
    grid->set_stroke(flex::Color(0.9f, 0.9f, 0.9f, 1.0f), 1.0f);
}

// Rendering
void Canvas::render(flex::Renderer& renderer) {
    // Apply camera transform to content layers
    auto camera = camera_transform();
    grid_layer_->set_transform(camera);
    content_root_->set_transform(camera);
    
    // UI overlay stays in screen space (no transform)
    
    instance_->render(renderer);
}

void Canvas::update(float dt) {
    instance_->advance(dt);
}

void Canvas::notify_layer_change() {
    if (layer_change_callback_) {
        layer_change_callback_();
    }
}

} // namespace meta_editor
