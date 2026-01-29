/*
 * Meta Editor - Selection Manager
 *
 * Manages selected nodes and provides selection UI.
 */

#pragma once

#include <flex.h>
#include <vector>
#include <functional>
#include <optional>
#include <unordered_set>

namespace meta_editor {

class Canvas;

// Handle positions for resize/transform
enum class HandleType {
    None = -1,
    TopLeft = 0,
    TopCenter,
    TopRight,
    RightCenter,
    BottomRight,
    BottomCenter,
    BottomLeft,
    LeftCenter,
    Rotate  // Rotation handle (above top-center)
};

class SelectionManager {
public:
    explicit SelectionManager(Canvas* canvas);

    // Selection
    void select(flex::Node* node);
    void add_to_selection(flex::Node* node);
    void remove_from_selection(flex::Node* node);
    void clear_selection();
    void select_all();

    // Query
    bool is_selected(flex::Node* node) const;
    const std::vector<flex::Node*>& selection() const { return selected_nodes_; }
    flex::Node* primary_selection() const;
    bool has_selection() const { return !selected_nodes_.empty(); }
    int selection_count() const { return (int)selected_nodes_.size(); }

    // Bounds
    flex::Bounds selection_bounds() const;
    flex::Vec2 selection_center() const;

    // Style - Get common style from selection (nullopt if mixed)
    std::optional<flex::Paint> get_common_fill() const;
    std::optional<flex::Paint> get_common_stroke() const;
    std::optional<float> get_common_stroke_width() const;

    // Style - Apply to selection
    void set_fill(const flex::Paint& paint);
    void set_stroke(const flex::Paint& paint, float width);
    void clear_fill();
    void clear_stroke();

    // Lock/Unlock nodes
    void lock_selection();
    void unlock_selection();
    bool is_locked(flex::Node* node) const;
    void toggle_lock(flex::Node* node);

    // Rendering
    void render_selection_indicators(flex::Renderer& renderer);

    // Handle hit testing (returns HandleType::None if no hit)
    HandleType hit_test_handle(const flex::Vec2& screen_pos, float threshold = 6.0f) const;
    flex::Vec2 get_handle_position(HandleType handle) const;

    // Callbacks
    using SelectionChangeCallback = std::function<void()>;
    void set_selection_change_callback(SelectionChangeCallback cb) {
        selection_change_callback_ = std::move(cb);
    }

private:
    void notify_selection_change();

    Canvas* canvas_;
    std::vector<flex::Node*> selected_nodes_;
    std::unordered_set<flex::Node*> locked_nodes_;
    SelectionChangeCallback selection_change_callback_;
};

} // namespace meta_editor
