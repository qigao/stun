/*
 * Flex Engine - Base Node Class
 *
 * Abstract base class for all scene graph nodes.
 * Implements common properties: position, opacity, visibility, tags.
 */

#pragma once

#include "flex/types.h"
#include "flex/event.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace flex {

// Forward declarations
class Group;
class Renderer;

// ============================================================================
// Node Types
// ============================================================================

enum class NodeType {
    Group,      // Container with children
    Shape,      // Geometry with fill/stroke
    Text,       // Typography
    Image,      // Raster image
    Svg,        // Vector graphic
    Instance,   // Component instance
    Solo,       // Single-child switcher
};

// ============================================================================
// Node - Base class for all scene graph elements
// ============================================================================

class Node : public std::enable_shared_from_this<Node> {
public:
    using Ptr = std::shared_ptr<Node>;

    Node() = default;
    virtual ~Node() = default;

    // Non-copyable, non-movable (use shared_ptr for ownership)
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // Type identification
    virtual NodeType type() const = 0;
    virtual const char* type_name() const = 0;

    // -------------------------------------------
    // Identity
    // -------------------------------------------

    const std::string& id() const { return id_; }
    void set_id(const std::string& id) { id_ = id; }

    const std::vector<std::string>& tags() const { return tags_; }
    void add_tag(const std::string& tag) { tags_.push_back(tag); }
    bool has_tag(const std::string& tag) const;

    // -------------------------------------------
    // Transform Properties
    // -------------------------------------------

    float x() const { return x_; }
    float y() const { return y_; }
    void set_x(float x) { x_ = x; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }
    void set_y(float y) { y_ = y; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }
    void set_position(float x, float y) { x_ = x; y_ = y; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }

    float scale_x() const { return scale_x_; }
    float scale_y() const { return scale_y_; }
    void set_scale(float sx, float sy) { scale_x_ = sx; scale_y_ = sy; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }
    void set_scale(float s) { scale_x_ = scale_y_ = s; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }

    float rotation() const { return rotation_; }
    void set_rotation(float degrees) { rotation_ = degrees; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }

    // -------------------------------------------
    // Visual Properties
    // -------------------------------------------

    float opacity() const { return opacity_; }
    void set_opacity(float o) { opacity_ = o; mark_dirty(DirtyFlags::Visual); }

    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; mark_dirty(DirtyFlags::Visual); }

    // -------------------------------------------
    // Effects (Shadow and Blur)
    // -------------------------------------------

    const Shadow& shadow() const { return shadow_; }
    void set_shadow(const Shadow& s) { shadow_ = s; mark_dirty(DirtyFlags::Visual); }
    void set_shadow(float ox, float oy, float blur, const Color& color) {
        shadow_ = Shadow(ox, oy, blur, color);
        mark_dirty(DirtyFlags::Visual);
    }
    bool has_shadow() const { return !shadow_.is_none(); }

    const BlurFilter& blur() const { return blur_; }
    void set_blur(const BlurFilter& b) { blur_ = b; mark_dirty(DirtyFlags::Visual); }
    void set_blur(float radius) { blur_ = BlurFilter(radius); mark_dirty(DirtyFlags::Visual); }
    bool has_blur() const { return !blur_.is_none(); }

    // -------------------------------------------
    // Layout Properties (for flex children)
    // -------------------------------------------

    float layout_width() const { return layout_width_; }
    float layout_height() const { return layout_height_; }
    void set_layout_width(float w) { layout_width_ = w; mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds); }
    void set_layout_height(float h) { layout_height_ = h; mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds); }
    void set_layout_size(float w, float h) { layout_width_ = w; layout_height_ = h; mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds); }

    float flex_grow() const { return flex_grow_; }
    float flex_shrink() const { return flex_shrink_; }
    float flex_basis() const { return flex_basis_; }
    void set_flex_grow(float g) { flex_grow_ = g; mark_dirty(DirtyFlags::Layout); }
    void set_flex_shrink(float s) { flex_shrink_ = s; mark_dirty(DirtyFlags::Layout); }
    void set_flex_basis(float b) { flex_basis_ = b; mark_dirty(DirtyFlags::Layout); }
    void set_flex(float grow, float shrink = 1.0f, float basis = 0.0f) {
        flex_grow_ = grow;
        flex_shrink_ = shrink;
        flex_basis_ = basis;
        mark_dirty(DirtyFlags::Layout);
    }

    AlignSelf align_self() const { return align_self_; }
    void set_align_self(AlignSelf a) { align_self_ = a; mark_dirty(DirtyFlags::Layout); }

    // -------------------------------------------
    // Dirty Flags (for optimization)
    // -------------------------------------------

    DirtyFlags dirty_flags() const { return dirty_flags_; }
    bool is_dirty() const { return dirty_flags_ != DirtyFlags::None; }
    bool is_dirty(DirtyFlags flag) const { return has_flag(dirty_flags_, flag); }
    void mark_dirty(DirtyFlags flags) { dirty_flags_ |= flags; propagate_dirty(); }
    void clear_dirty() { dirty_flags_ = DirtyFlags::None; }
    void clear_dirty(DirtyFlags flags) { dirty_flags_ &= ~flags; }

    // -------------------------------------------
    // Culling (for rendering optimization)
    // -------------------------------------------

    // Quick check if node should be rendered at all
    bool should_render() const { return visible_ && opacity_ > 0.0f; }

    // Check culling against viewport bounds
    CullResult cull(const Bounds& viewport) const;

    // Check if node's bounds intersect with viewport
    bool intersects_viewport(const Bounds& viewport) const;

    // -------------------------------------------
    // Hierarchy
    // -------------------------------------------

    Node* parent() const { return parent_; }
    virtual bool is_group() const { return false; }

    // Find child node by ID (returns nullptr for non-Group nodes)
    virtual Node* find(const std::string& id) { return nullptr; }

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    virtual void render(Renderer& renderer) = 0;

    // -------------------------------------------
    // Hit Testing and Events
    // -------------------------------------------

    // Get node bounds (override in subclasses)
    virtual Bounds bounds() const { return Bounds{x_, y_, 0, 0}; }

    // Test if point is inside node
    virtual bool hit_test(float px, float py) const;

    // Event callbacks - Pointer
    void on_pointer_down(PointerEventCallback callback) { on_pointer_down_ = std::move(callback); }
    void on_pointer_up(PointerEventCallback callback) { on_pointer_up_ = std::move(callback); }
    void on_pointer_move(PointerEventCallback callback) { on_pointer_move_ = std::move(callback); }
    void on_hover_enter(PointerEventCallback callback) { on_hover_enter_ = std::move(callback); }
    void on_hover_leave(PointerEventCallback callback) { on_hover_leave_ = std::move(callback); }
    void on_click(ClickCallback callback) { on_click_ = std::move(callback); }

    // Event callbacks - Keyboard
    void on_key_down(KeyEventCallback callback) { on_key_down_ = std::move(callback); }
    void on_key_up(KeyEventCallback callback) { on_key_up_ = std::move(callback); }
    void on_focus(FocusCallback callback) { on_focus_ = std::move(callback); }

    // Fire events (called by Instance during event propagation)
    void fire_pointer_down(PointerEvent& event);
    void fire_pointer_up(PointerEvent& event);
    void fire_pointer_move(PointerEvent& event);
    void fire_hover_enter(PointerEvent& event);
    void fire_hover_leave(PointerEvent& event);
    void fire_click();
    void fire_key_down(KeyEvent& event);
    void fire_key_up(KeyEvent& event);
    void fire_focus(bool gained);

    // Check if node has event handlers
    bool has_pointer_handlers() const;
    bool has_key_handlers() const;

    // -------------------------------------------
    // Focus Management
    // -------------------------------------------

    bool focusable() const { return focusable_; }
    void set_focusable(bool f) { focusable_ = f; }

    bool focused() const { return focused_; }
    void set_focused(bool f);  // Internal: use Instance::set_focus() instead

protected:
    friend class Group;

    // Propagate dirty to parent (layout changes affect parent)
    void propagate_dirty();

    std::string id_;
    std::vector<std::string> tags_;

    // Transform
    float x_ = 0, y_ = 0;
    float scale_x_ = 1, scale_y_ = 1;
    float rotation_ = 0;  // degrees

    // Visual
    float opacity_ = 1.0f;
    bool visible_ = true;

    // Effects
    Shadow shadow_;
    BlurFilter blur_;

    // Layout (for flex children)
    float layout_width_ = 0;    // 0 = auto (use bounds)
    float layout_height_ = 0;   // 0 = auto (use bounds)
    float flex_grow_ = 0;       // Grow factor
    float flex_shrink_ = 1;     // Shrink factor
    float flex_basis_ = 0;      // Initial main size (0 = auto)
    AlignSelf align_self_ = AlignSelf::Auto;

    // Dirty flags (for optimization)
    DirtyFlags dirty_flags_ = DirtyFlags::All;  // Start dirty

    // Hierarchy (set by parent)
    Node* parent_ = nullptr;

    // Event callbacks - Pointer
    PointerEventCallback on_pointer_down_;
    PointerEventCallback on_pointer_up_;
    PointerEventCallback on_pointer_move_;
    PointerEventCallback on_hover_enter_;
    PointerEventCallback on_hover_leave_;
    ClickCallback on_click_;

    // Event callbacks - Keyboard
    KeyEventCallback on_key_down_;
    KeyEventCallback on_key_up_;
    FocusCallback on_focus_;

    // Focus state
    bool focusable_ = false;
    bool focused_ = false;
};

} // namespace flex
