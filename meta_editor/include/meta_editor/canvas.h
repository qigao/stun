/*
 * Meta Editor - Canvas
 *
 * Core canvas class managing layers, camera, and rendering.
 */

#pragma once

#include <flex.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace meta_editor {

class Canvas {
public:
    Canvas(float width, float height);
    ~Canvas();

    // Dimensions
    void set_size(float width, float height);
    float width() const;
    float height() const;

    // Layer Management (layers are flex::Group*)
    flex::Group* create_layer(const std::string& name, flex::Group* parent = nullptr);
    bool delete_layer(flex::Group* layer);  // Returns false if layer cannot be deleted
    void reorder_layer(flex::Group* layer, int new_index);
    flex::Group* get_layer_by_name(const std::string& name);
    std::vector<flex::Group*> get_all_layers() const;

    // Layer Properties
    void set_layer_visible(flex::Group* layer, bool visible);
    void set_layer_locked(flex::Group* layer, bool locked);
    void set_layer_opacity(flex::Group* layer, float opacity);
    bool is_layer_locked(flex::Group* layer) const;
    
    // Camera Control
    void pan(float dx, float dy);
    void zoom(float delta);
    void zoom_at(float screen_x, float screen_y, float delta);
    void reset_camera();
    void fit_to_view();
    
    // Coordinate Conversion
    flex::Vec2 screen_to_world(float screen_x, float screen_y) const;
    flex::Vec2 world_to_screen(float world_x, float world_y) const;
    flex::Vec2 screen_to_world(const flex::Vec2& screen_pos) const;
    flex::Vec2 world_to_screen(const flex::Vec2& world_pos) const;
    
    // Hit Testing
    flex::Node* hit_test(float screen_x, float screen_y);
    flex::Node* hit_test(const flex::Vec2& screen_pos);
    
    // Rendering
    void render(flex::Renderer& renderer);
    void update(float dt);
    
    // Grid
    void set_grid_visible(bool visible);
    void set_grid_size(float size);
    void set_snap_to_grid(bool enabled);
    bool is_snap_to_grid() const { return snap_to_grid_; }
    float grid_size() const { return grid_size_; }
    flex::Vec2 snap_to_grid(const flex::Vec2& pos) const;
    
    // Access
    flex::Instance* instance() { return instance_.get(); }
    flex::Scene* scene() { return scene_; }
    flex::Group* content_root() { return content_root_; }
    flex::Group* ui_overlay_root() { return ui_overlay_root_; }
    
    float camera_pan_x() const { return camera_pan_x_; }
    float camera_pan_y() const { return camera_pan_y_; }
    float camera_zoom() const { return camera_zoom_; }
    flex::Transform camera_transform() const;
    
    // Callbacks
    using LayerChangeCallback = std::function<void()>;
    void set_layer_change_callback(LayerChangeCallback cb) { layer_change_callback_ = std::move(cb); }

private:
    flex::Transform inverse_camera_transform() const;
    void rebuild_grid();
    void notify_layer_change();
    
    flex::Instance::Ptr instance_;
    flex::Scene* scene_;
    
    // Layer structure
    flex::Group* content_root_;      // All content (affected by camera)
    flex::Group* ui_overlay_root_;   // UI overlay (screen space)
    flex::Group* grid_layer_;        // Background grid
    
    // Camera state
    float camera_pan_x_ = 0;
    float camera_pan_y_ = 0;
    float camera_zoom_ = 1.0f;
    float min_zoom_ = 0.05f;
    float max_zoom_ = 20.0f;
    
    // Grid settings
    bool grid_visible_ = true;
    float grid_size_ = 50.0f;
    bool snap_to_grid_ = false;
    
    // Layer metadata
    std::unordered_map<flex::Group*, bool> locked_layers_;
    
    // Callbacks
    LayerChangeCallback layer_change_callback_;
};

} // namespace meta_editor
