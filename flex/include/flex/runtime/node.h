/*
 * Flex Engine - Base Node Class
 *
 * Abstract base class for all scene graph nodes.
 * Implements common properties: position, opacity, visibility, tags.
 *
 * Memory Management:
 * - Default: shared_ptr for automatic memory management
 * - Arena mode: raw pointers with ArenaAllocator for high-performance scenarios
 * - Layout data: on-demand allocation (only when flex properties are used)
 */

#pragma once

#include "types.h"
#include "layout_data.h"
#include "flex/runtime/event.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace flex {

// Forward declarations
class Group;
class Renderer;
class Machine;
class ArenaAllocator;

// Opaque handle for backend-specific cached objects (retained mode)
using PaintHandle = void*;

// ============================================================================
// Node Types
// ============================================================================

enum class NodeType {
    Group,
    Shape,
    Text,
    Image,
    Svg,
    Instance,
    Solo,
};

// ============================================================================
// Node - Base class for all scene graph elements
// ============================================================================

class Node {
public:
    using Ptr = Node*;

    Node();
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    virtual NodeType type() const = 0;
    virtual const char* type_name() const = 0;

    // -------------------------------------------
    // Identity
    // -------------------------------------------

    const std::string& id() const { return id_; }
    void set_id(const std::string& id) { id_ = id; }

    const std::vector<std::string>& tags() const {
        static const std::vector<std::string> empty;
        return tags_ ? *tags_ : empty;
    }
    void add_tag(const std::string& tag) {
        if (!tags_) tags_ = std::vector<std::string>();
        tags_->push_back(tag);
    }
    bool has_tag(const std::string& tag) const {
        return tags_ && std::find(tags_->begin(), tags_->end(), tag) != tags_->end();
    }

    // -------------------------------------------
    // Transform Properties
    // -------------------------------------------

    float x() const { return x_; }
    float y() const { return y_; }

    void set_x(float x) {
        if (x_ == x) return;
        x_ = x;
        has_manual_transform_ = false;
        mark_dirty_internal(DirtyFlags::Transform | DirtyFlags::Bounds);
    }

    void set_y(float y) {
        if (y_ == y) return;
        y_ = y;
        has_manual_transform_ = false;
        mark_dirty_internal(DirtyFlags::Transform | DirtyFlags::Bounds);
    }

    void set_position(float x, float y) {
        if (x_ == x && y_ == y) return;
        x_ = x;
        y_ = y;
        has_manual_transform_ = false;
        mark_dirty_internal(DirtyFlags::Transform | DirtyFlags::Bounds);
    }

    float scale_x() const { return scale_x_; }
    float scale_y() const { return scale_y_; }
    void set_scale(float sx, float sy) {
        if (scale_x_ == sx && scale_y_ == sy) return;
        scale_x_ = sx; scale_y_ = sy;
        has_manual_transform_ = false;
        mark_dirty_internal(DirtyFlags::Transform | DirtyFlags::Bounds);
    }
    void set_scale(float s) { set_scale(s, s); }

    float rotation() const { return rotation_; }
    void set_rotation(float degrees) {
        if (rotation_ == degrees) return;
        rotation_ = degrees;
        has_manual_transform_ = false;
        mark_dirty_internal(DirtyFlags::Transform | DirtyFlags::Bounds);
    }

    const Transform& transform() const { return local_transform(); }
    void set_transform(const Transform& t) {
        // Transform comparison is slightly expensive, but usually worth it to avoid layout drift
        if (local_transform_ == t && has_manual_transform_) return;
        local_transform_ = t;
        has_manual_transform_ = true;
        mark_dirty_internal(DirtyFlags::Transform | DirtyFlags::Bounds);
    }

    // -------------------------------------------
    // Visual Properties
    // -------------------------------------------

    float opacity() const { return opacity_; }
    void set_opacity(float o) { 
        if (opacity_ == o) return;
        opacity_ = o; 
        mark_dirty_internal(DirtyFlags::Visual); 
    }

    bool visible() const { return visible_; }
    void set_visible(bool v) { 
        if (visible_ == v) return;
        visible_ = v; 
        mark_dirty_internal(DirtyFlags::Visual | DirtyFlags::Layout | DirtyFlags::Bounds); 
    }

    // -------------------------------------------
    // Effects (Shadow and Blur)
    // -------------------------------------------

    const Shadow& shadow() const {
        static Shadow none;
        return shadow_ ? *shadow_ : none;
    }
    void set_shadow(const Shadow& s) { shadow_ = s; mark_dirty(DirtyFlags::Visual); }
    void set_shadow(float ox, float oy, float blur, const Color& color) {
        shadow_ = Shadow(ox, oy, blur, color);
        mark_dirty(DirtyFlags::Visual);
    }
    bool has_shadow() const { return shadow_ && !shadow_->is_none(); }

    const BlurFilter& blur() const {
        static BlurFilter none;
        return blur_ ? *blur_ : none;
    }
    void set_blur(const BlurFilter& b) { blur_ = b; mark_dirty(DirtyFlags::Visual); }
    void set_blur(float radius) { blur_ = BlurFilter(radius); mark_dirty(DirtyFlags::Visual); }
    bool has_blur() const { return blur_ && !blur_->is_none(); }

    // -------------------------------------------
    // Layout Properties (on-demand allocation)
    // -------------------------------------------

    float layout_width() const { return layout_ ? layout_->width : 0; }
    float layout_height() const { return layout_ ? layout_->height : 0; }
    void set_layout_width(float w) { 
        if (layout_ && layout_->width == w && !layout_->width_is_percent) return;
        ensure_layout(); 
        layout_->width = w; 
        layout_->width_is_percent = false;
        mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds); 
    }
    void set_layout_height(float h) { 
        if (layout_ && layout_->height == h && !layout_->height_is_percent) return;
        ensure_layout(); 
        layout_->height = h; 
        layout_->height_is_percent = false;
        mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds); 
    }
    void set_layout_size(float w, float h) {
        if (layout_ && layout_->width == w && layout_->height == h && !layout_->width_is_percent && !layout_->height_is_percent) return;
        ensure_layout();
        layout_->width = w;
        layout_->height = h;
        layout_->width_is_percent = false;
        layout_->height_is_percent = false;
        mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds);
    }

    float flex_grow() const { return layout_ ? layout_->flex_grow : 0; }
    float flex_shrink() const { return layout_ ? layout_->flex_shrink : 1; }
    float flex_basis() const { return layout_ ? layout_->flex_basis : 0; }
    void set_flex_grow(float g) { if (layout_ && layout_->flex_grow == g) return; ensure_layout(); layout_->flex_grow = g; mark_dirty(DirtyFlags::Layout); }
    void set_flex_shrink(float s) { if (layout_ && layout_->flex_shrink == s) return; ensure_layout(); layout_->flex_shrink = s; mark_dirty(DirtyFlags::Layout); }
    void set_flex_basis(float b) { if (layout_ && layout_->flex_basis == b) return; ensure_layout(); layout_->flex_basis = b; mark_dirty(DirtyFlags::Layout); }
    void set_flex(float grow, float shrink = 1.0f, float basis = 0.0f) {
        if (layout_ && layout_->flex_grow == grow && layout_->flex_shrink == shrink && layout_->flex_basis == basis) return;
        ensure_layout();
        layout_->flex_grow = grow;
        layout_->flex_shrink = shrink;
        layout_->flex_basis = basis;
        mark_dirty(DirtyFlags::Layout);
    }

    AlignSelf align_self() const { return layout_ ? layout_->align_self : AlignSelf::Auto; }
    void set_align_self(AlignSelf a) { if (layout_ && layout_->align_self == a) return; ensure_layout(); layout_->align_self = a; mark_dirty(DirtyFlags::Layout); }

    bool position_absolute() const {
        if (!layout_) return false;
        return layout_->position_absolute ||
               layout_->position_mode == PositionMode::Absolute ||
               layout_->position_mode == PositionMode::Fixed;
    }
    void set_position_absolute(bool a) { if (layout_ && layout_->position_absolute == a) return; ensure_layout(); layout_->position_absolute = a; mark_dirty(DirtyFlags::Layout); }

    Anchor anchor() const { return layout_ ? layout_->anchor : Anchor::TopLeft; }
    void set_anchor(Anchor a) { if (layout_ && layout_->anchor == a) return; ensure_layout(); layout_->anchor = a; mark_dirty(DirtyFlags::Transform | DirtyFlags::Bounds); }

    // Extended Layout Properties
    PositionMode position_mode() const { return layout_ ? layout_->position_mode : PositionMode::Static; }
    void set_position_mode(PositionMode m) {
        if (layout_ && layout_->position_mode == m) return;
        ensure_layout();
        layout_->position_mode = m;
        layout_->position_absolute = (m == PositionMode::Absolute || m == PositionMode::Fixed);
        mark_dirty(DirtyFlags::Layout);
    }

    float position_top() const { return layout_ ? layout_->position_top : NAN; }
    float position_right() const { return layout_ ? layout_->position_right : NAN; }
    float position_bottom() const { return layout_ ? layout_->position_bottom : NAN; }
    float position_left() const { return layout_ ? layout_->position_left : NAN; }
    void set_position_top(float v) { if (layout_ && layout_->position_top == v) return; ensure_layout(); layout_->position_top = v; mark_dirty(DirtyFlags::Layout); }
    void set_position_right(float v) { if (layout_ && layout_->position_right == v) return; ensure_layout(); layout_->position_right = v; mark_dirty(DirtyFlags::Layout); }
    void set_position_bottom(float v) { if (layout_ && layout_->position_bottom == v) return; ensure_layout(); layout_->position_bottom = v; mark_dirty(DirtyFlags::Layout); }
    void set_position_left(float v) { if (layout_ && layout_->position_left == v) return; ensure_layout(); layout_->position_left = v; mark_dirty(DirtyFlags::Layout); }

    void set_position_offsets(float top, float right, float bottom, float left) {
        ensure_layout();
        layout_->position_top = top;
        layout_->position_right = right;
        layout_->position_bottom = bottom;
        layout_->position_left = left;
        mark_dirty(DirtyFlags::Layout);
    }

    int z_index() const { return layout_ ? layout_->z_index : 0; }
    void set_z_index(int z) { ensure_layout(); layout_->z_index = z; mark_dirty(DirtyFlags::Visual); }

    BoxSizing box_sizing() const { return layout_ ? layout_->box_sizing : BoxSizing::ContentBox; }
    void set_box_sizing(BoxSizing b) { ensure_layout(); layout_->box_sizing = b; mark_dirty(DirtyFlags::Layout); }

    bool width_is_percent() const { return layout_ && layout_->width_is_percent; }
    bool height_is_percent() const { return layout_ && layout_->height_is_percent; }
    void set_width_percent(float percent) {
        ensure_layout();
        layout_->width = percent;
        layout_->width_is_percent = true;
        mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds);
    }
    void set_height_percent(float percent) {
        ensure_layout();
        layout_->height = percent;
        layout_->height_is_percent = true;
        mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds);
    }

    float margin_top() const { return layout_ ? layout_->margin[0] : 0; }
    float margin_right() const { return layout_ ? layout_->margin[1] : 0; }
    float margin_bottom() const { return layout_ ? layout_->margin[2] : 0; }
    float margin_left() const { return layout_ ? layout_->margin[3] : 0; }
    void set_margin(float all) {
        ensure_layout();
        layout_->margin[0] = layout_->margin[1] = layout_->margin[2] = layout_->margin[3] = all;
        mark_dirty(DirtyFlags::Layout);
    }
    void set_margin(float top, float right, float bottom, float left) {
        ensure_layout();
        layout_->margin[0] = top;
        layout_->margin[1] = right;
        layout_->margin[2] = bottom;
        layout_->margin[3] = left;
        mark_dirty(DirtyFlags::Layout);
    }

    float border_width_top() const { return layout_ ? layout_->border_width[0] : 0; }
    float border_width_right() const { return layout_ ? layout_->border_width[1] : 0; }
    float border_width_bottom() const { return layout_ ? layout_->border_width[2] : 0; }
    float border_width_left() const { return layout_ ? layout_->border_width[3] : 0; }
    void set_border_width(float all) {
        ensure_layout();
        layout_->border_width[0] = layout_->border_width[1] = layout_->border_width[2] = layout_->border_width[3] = all;
        mark_dirty(DirtyFlags::Layout);
    }
    void set_border_width(float top, float right, float bottom, float left) {
        ensure_layout();
        layout_->border_width[0] = top;
        layout_->border_width[1] = right;
        layout_->border_width[2] = bottom;
        layout_->border_width[3] = left;
        mark_dirty(DirtyFlags::Layout);
    }

    // Check if layout data is allocated
    bool has_layout() const { return layout_ != nullptr; }
    LayoutData* layout_data() { return layout_.get(); }
    const LayoutData* layout_data() const { return layout_.get(); }

    // -------------------------------------------
    // Matrix Transforms
    // -------------------------------------------

    const Transform& local_transform() const {
        if (is_dirty(DirtyFlags::Transform)) {
            const_cast<Node*>(this)->update_local_transform();
        }
        return local_transform_;
    }

    const Transform& world_transform() const {
        if (is_dirty(DirtyFlags::Transform)) {
            const_cast<Node*>(this)->update_world_transform();
        }
        return world_transform_;
    }

    Vec2 to_local(const Vec2& world_pos) const { return world_transform().inverse() * world_pos; }
    Vec2 to_world(const Vec2& local_pos) const { return world_transform() * local_pos; }

    // -------------------------------------------
    // Dirty Flags
    // -------------------------------------------

    DirtyFlags dirty_flags() const { return dirty_flags_; }
    bool is_dirty() const { return dirty_flags_ != DirtyFlags::None; }
    bool is_dirty(DirtyFlags flag) const { return has_flag(dirty_flags_, flag); }
    virtual void mark_dirty(DirtyFlags flags);
    void clear_dirty() { dirty_flags_ = DirtyFlags::None; }
    void clear_dirty(DirtyFlags flags) { dirty_flags_ &= ~flags; }

    // -------------------------------------------
    // Batch Updates
    // -------------------------------------------

    void begin_batch() { batch_mode_ = true; }
    void end_batch() {
        batch_mode_ = false;
        if (pending_dirty_flags_ != DirtyFlags::None) {
            mark_dirty(pending_dirty_flags_);
            pending_dirty_flags_ = DirtyFlags::None;
        }
    }
    bool is_batching() const { return batch_mode_; }

    // -------------------------------------------
    // Culling
    // -------------------------------------------

    CullResult cull(const Bounds& viewport) const {
        if (!visible_) return CullResult::Hidden;
        if (opacity_ <= 0.0f) return CullResult::Transparent;
        if (!intersects_viewport(viewport)) return CullResult::OutOfView;
        return CullResult::Visible;
    }

    bool intersects_viewport(const Bounds& viewport) const;

    // -------------------------------------------
    // Hierarchy
    // -------------------------------------------

    Node* parent() const { return parent_; }
    virtual bool is_group() const { return false; }
    virtual Node* find(const std::string& id) { return nullptr; }

    // -------------------------------------------
    // Animation Property Dispatch
    // -------------------------------------------

    // Set animated property by ID - returns true if handled
    // Base class handles: x, y, rotation, scale, scaleX, scaleY, opacity, visible
    // Subclasses override to handle their specific properties
    virtual bool set_animated_property(PropertyID pid, const AnimValue& value);

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    virtual void render(Renderer& renderer) = 0;

    // -------------------------------------------
    // Hit Testing and Events
    // -------------------------------------------

    Bounds bounds() const {
        if (is_dirty(DirtyFlags::Bounds)) {
            cached_bounds_ = compute_bounds();
            const_cast<Node*>(this)->clear_dirty(DirtyFlags::Bounds);
        }
        return cached_bounds_;
    }

    Bounds world_bounds() const {
        if (is_dirty(DirtyFlags::WorldBounds | DirtyFlags::Transform | DirtyFlags::Bounds)) {
            cached_world_bounds_ = bounds().transformed(world_transform());
            const_cast<Node*>(this)->clear_dirty(DirtyFlags::WorldBounds);
        }
        return cached_world_bounds_;
    }

    virtual Bounds compute_bounds() const { return Bounds{0, 0, 0, 0}; }
    virtual bool hit_test(float px, float py) const;

    // Event callbacks - Pointer
    void on_pointer_down(PointerEventCallback callback) { ensure_events(); events_->on_pointer_down = std::move(callback); }
    void on_pointer_up(PointerEventCallback callback) { ensure_events(); events_->on_pointer_up = std::move(callback); }
    void on_pointer_move(PointerEventCallback callback) { ensure_events(); events_->on_pointer_move = std::move(callback); }
    void on_hover_enter(PointerEventCallback callback) { ensure_events(); events_->on_hover_enter = std::move(callback); }
    void on_hover_leave(PointerEventCallback callback) { ensure_events(); events_->on_hover_leave = std::move(callback); }
    void on_click(ClickCallback callback) { ensure_events(); events_->on_click = std::move(callback); }

    // Event callbacks - Keyboard
    void on_key_down(KeyEventCallback callback) { ensure_events(); events_->on_key_down = std::move(callback); }
    void on_key_up(KeyEventCallback callback) { ensure_events(); events_->on_key_up = std::move(callback); }
    void on_focus(FocusCallback callback) { ensure_events(); events_->on_focus = std::move(callback); }

    // Fire events
    void fire_pointer_down(PointerEvent& event);
    void fire_pointer_up(PointerEvent& event);
    void fire_pointer_move(PointerEvent& event);
    void fire_hover_enter(PointerEvent& event);
    void fire_hover_leave(PointerEvent& event);
    void fire_click();
    void fire_key_down(KeyEvent& event);
    void fire_key_up(KeyEvent& event);
    void fire_focus(bool gained);

    bool has_pointer_handlers() const;
    bool has_key_handlers() const;

    // -------------------------------------------
    // Focus Management
    // -------------------------------------------

    bool focusable() const { return focusable_; }
    void set_focusable(bool f) { focusable_ = f; }
    bool focused() const { return focused_; }
    void set_focused(bool f);

    // -------------------------------------------
    // FSM and Pseudo-Class Styles
    // -------------------------------------------

    using PseudoClassStyleMap = std::unordered_map<std::string, class PseudoClassStyle>;
    PseudoClassStyleMap* pseudo_class_styles() const { return pseudo_styles_.get(); }
    void add_pseudo_class_style(const std::string& name, const class PseudoClassStyle& style);

    void* fsm_instance() const { return fsm_instance_; }
    void set_fsm_instance(void* fsm) { fsm_instance_ = fsm; }

    template<typename FsmType>
    FsmType* get_fsm() const { return static_cast<FsmType*>(fsm_instance_); }

    template<typename EventType>
    void dispatch_fsm_event(const EventType& event);

    Node* find_by_path(const std::string& path);

    // -------------------------------------------
    // Retained Mode Cache
    // -------------------------------------------

    PaintHandle cached_paint() const { return cached_paint_; }
    void set_cached_paint(PaintHandle paint) { cached_paint_ = paint; }
    void invalidate_cache() { cached_paint_ = nullptr; }
    bool needs_rebuild() const { return cached_paint_ == nullptr || is_dirty(DirtyFlags::Content); }

protected:
    friend class Group;

    void propagate_dirty();
    void ensure_events();
    void ensure_layout() { if (!layout_) layout_ = std::make_unique<LayoutData>(); }

    // Identity
    std::string id_;
    std::optional<std::vector<std::string>> tags_;

    // Transform
    float x_ = 0, y_ = 0;
    float scale_x_ = 1, scale_y_ = 1;
    float rotation_ = 0;

    // Visual
    float opacity_ = 1.0f;
    bool visible_ = true;

    // Effects (optional)
    std::optional<Shadow> shadow_;
    std::optional<BlurFilter> blur_;

    // Layout (on-demand allocation - saves ~80 bytes per node)
    std::unique_ptr<LayoutData> layout_;

    // Dirty flags
    DirtyFlags dirty_flags_ = DirtyFlags::All;
    bool batch_mode_ = false;
    DirtyFlags pending_dirty_flags_ = DirtyFlags::None;
    bool has_manual_transform_ = false;

    // Cached bounds
    mutable Bounds cached_bounds_;
    mutable Bounds cached_world_bounds_;

    void mark_dirty_internal(DirtyFlags flags) {
        if (batch_mode_) {
            pending_dirty_flags_ |= flags;
        } else {
            mark_dirty(flags);
        }
    }

    // Cached transforms
    Transform local_transform_ = Transform::Identity();
    Transform world_transform_ = Transform::Identity();

    void update_local_transform();
    void update_world_transform();

    // Hierarchy
    Node* parent_ = nullptr;

    // Events (on-demand allocation)
    EventDispatcher* events_ = nullptr;

    // Focus
    bool focusable_ = false;
    bool focused_ = false;

    // FSM
    std::unique_ptr<PseudoClassStyleMap> pseudo_styles_;
    void* fsm_instance_ = nullptr;

    // Retained mode cache
    PaintHandle cached_paint_ = nullptr;
};

} // namespace flex
