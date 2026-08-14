/*
 * Flex Engine - Timeline Animation System
 *
 * Keyframe-based animation with tracks, triggers, easing, and blending.
 * Optimized with arena allocator for zero-allocation animation sampling.
 */

#pragma once
#include "flex/animation/keyframe.h"
#include "flex/animation/numeric_expression.h"
#include "flex/animation/playback.h"
#include "flex/core/animation_program.h"
#include "flex/core/types.h"
#include "flex/core/allocator.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <variant>

namespace flex {

// Forward declarations
class Node;

// ============================================================================
// Animation Value Types (using std::variant for type safety)
// ============================================================================

// ============================================================================
// Loop Mode
// ============================================================================

using LoopMode = animation::LoopMode;

// ============================================================================
// Blend Mode - How animations combine
// ============================================================================

enum class BlendMode {
    Override,   // Replace any existing animation (default)
    Additive,   // Add to existing animation values
    Multiply,   // Multiply with existing values (for opacity, scale)
};

// ============================================================================
// Trigger - Time-based event
// ============================================================================

struct Trigger {
    float time = 0;
    std::string event;  // Event name to fire
};

// Trigger callback receives the event name
using TriggerCallback = std::function<void(const std::string& event)>;

// ============================================================================
// Keyframe - Single animation keyframe
// ============================================================================

// ============================================================================
// Track - Single property animation channel
// ============================================================================

class Track {
public:
    using SharedPtr = std::shared_ptr<Track>;
    using Ptr = SharedPtr;

    Track(const char* property, ArenaAllocator& alloc);
    ~Track() = default;

    // Factory
    static SharedPtr create(const char* property, ArenaAllocator& alloc) {
        return std::make_shared<Track>(property, alloc);
    }

    // Property path (e.g., "opacity", "x", "fill.color")
    const char* property() const { return property_.c_str(); }

    // Keyframe management
    void add_keyframe(float time, float value, Easing easing = Easing::linear());
    void add_keyframe(float time, const char* value, Easing easing = Easing::linear());
    void add_keyframe(float time, const Color& value, Easing easing = Easing::linear());
    void add_keyframe(float time, const Vec2& value, Easing easing = Easing::linear());
    void add_spatial_keyframe(float time, const Vec2& value,
                              const Vec2& in_tangent,
                              const Vec2& out_tangent,
                              Easing easing = Easing::linear());
    void clear_keyframes();
    size_t keyframe_count() const { return keyframes_.size(); }
    const PoolVector<Keyframe>& keyframes() const { return keyframes_; }

    // Override float keyframe interpolation with a MIR numeric expression.
    void set_numeric_expression(const std::string& expression);
    void clear_numeric_expression();
    bool has_numeric_expression() const { return numeric_expression_ != nullptr; }

    void set_spatial_interpolation(SpatialInterpolation interpolation);
    SpatialInterpolation spatial_interpolation() const { return spatial_interpolation_; }

    // Sample value at time
    AnimValue sample(float time) const;

    // Get duration (time of last keyframe)
    float duration() const;

private:
    friend class Timeline;
    friend class TimelinePlayer;

    struct ProgramRevisionState;

    void bind_program_revision(
        const std::shared_ptr<ProgramRevisionState>& revision);
    void mark_program_dirty();

    std::string property_;  // Own the string to avoid dangling pointer
    std::string target_id_;
    PropertyID property_id_ = PropertyID::Unknown;
    bool has_target_selector_ = false;
    PoolVector<Keyframe> keyframes_;  // Uses arena allocator
    std::shared_ptr<animation::NumericExpression> numeric_expression_;
    SpatialInterpolation spatial_interpolation_ = SpatialInterpolation::Linear;
    std::weak_ptr<ProgramRevisionState> program_revision_;

};

// ============================================================================
// Timeline - Collection of tracks with playback control
// ============================================================================

class Timeline {
public:
    using SharedPtr = std::shared_ptr<Timeline>;
    using Ptr = SharedPtr;

    Timeline(const char* name, ArenaAllocator& alloc);
    ~Timeline() = default;

    // Factory
    static SharedPtr create(const char* name, ArenaAllocator& alloc) {
        return std::make_shared<Timeline>(name, alloc);
    }

    // Identity
    const char* name() const { return name_.c_str(); }

    // Get raw pointer (for TimelinePlayer constructor)
    Timeline* get_ptr() { return this; }

    // Duration (explicit or auto from tracks)
    float duration() const { return duration_ > 0 ? duration_ : auto_duration(); }
    void set_duration(float d) { duration_ = d; }

    // Loop mode
    LoopMode loop_mode() const { return loop_mode_; }
    void set_loop_mode(LoopMode mode) { loop_mode_ = mode; }

    // Speed multiplier
    float speed() const { return speed_; }
    void set_speed(float s) { speed_ = s; }

    // Track management
    Track::SharedPtr add_track(const char* property);
    Track* get_track(const char* property) const;
    const PoolVector<Track::SharedPtr>& tracks() const { return tracks_; }

    /**
     * Compile or return the cached immutable track operation table.
     *
     * The returned reference is borrowed and remains valid until this Timeline
     * is destroyed or a later program() call observes a Track mutation.
     * Compile complexity is O(track_count + keyframe_count) time and space;
     * cached lookup is O(1).
     */
    const AnimationProgram& program() const;

    // Trigger management
    void add_trigger(float time, const char* event);
    void clear_triggers();
    const PoolVector<Trigger>& triggers() const;
    size_t trigger_count() const { return triggers_.size(); }

    // Apply timeline values to a node at given time
    void apply(Node* target, float time) const;

private:
    friend class TimelinePlayer;

    std::weak_ptr<const void> lifetime_token() const {
        if (lifetime_token_.expired()) {
            lifetime_owner_ = std::make_shared<int>(0);
            lifetime_token_ = lifetime_owner_;
        }
        return lifetime_token_;
    }

    std::shared_ptr<Track::ProgramRevisionState> program_revision_;
    std::string name_;  // Own the string to avoid dangling pointer
    PoolVector<Track::SharedPtr> tracks_;  // Uses arena allocator
    mutable PoolVector<Trigger> triggers_;   // Uses arena allocator (mutable for lazy sorting)
    mutable bool triggers_sorted_ = true;    // Track if triggers are sorted
    float duration_ = 0;  // 0 = auto from tracks
    LoopMode loop_mode_ = LoopMode::Once;
    float speed_ = 1.0f;
    ArenaAllocator* allocator_;  // For creating tracks
    // Players borrow Timeline; this marker lets them reject access after destruction.
    mutable std::shared_ptr<const void> lifetime_owner_;
    mutable std::weak_ptr<const void> lifetime_token_;
    mutable std::unique_ptr<const AnimationProgram> compiled_program_;

    float auto_duration() const;
};

// ============================================================================
// TimelinePlayer - Runtime playback state with blending support
// ============================================================================

class TimelinePlayer {
public:
    TimelinePlayer(Timeline* timeline, Node* target);
    ~TimelinePlayer() = default;

    // Access
    Timeline* timeline() const { return timeline_alive() ? timeline_ : nullptr; }
    Node* target() const { return target_alive() ? target_ : nullptr; }

    // Playback state
    bool is_playing() const {
        return timeline_alive() && target_alive() && playback_.playing();
    }
    bool is_finished() const {
        return !timeline_alive() || !target_alive() || playback_.finished();
    }
    float current_time() const { return playback_.time(); }
    float normalized_time() const;  // 0-1 progress

    // Control
    void play();
    void pause();
    void stop();
    void seek(float time);

    // Blending
    BlendMode blend_mode() const { return blend_mode_; }
    void set_blend_mode(BlendMode mode) { blend_mode_ = mode; }

    float blend_weight() const { return blend_weight_; }
    void set_blend_weight(float weight) { blend_weight_ = (std::max)(0.0f, (std::min)(1.0f, weight)); }

    // Start a fade to target weight over duration
    void fade_to(float target_weight, float duration) {
        fade_start_weight_ = blend_weight_;
        fade_target_weight_ = (std::max)(0.0f, (std::min)(1.0f, target_weight));
        fade_duration_ = duration;
        fade_time_ = 0;
    }

    // Layer priority (higher = applied later)
    int layer() const { return layer_; }
    void set_layer(int layer) { layer_ = layer; }

    // Trigger callback
    void set_trigger_callback(TriggerCallback callback) { trigger_callback_ = std::move(callback); }

    // Duration override (0 = use timeline duration)
    void set_duration_override(float d) { duration_override_ = d; }
    float duration_override() const { return duration_override_; }

    // Per-player speed multiplier. Timeline::speed() remains the shared base speed.
    float speed() const { return speed_multiplier_; }
    void set_speed(float s) { speed_multiplier_ = s; }

    // Update (called each frame)
    // Returns true if still playing
    bool advance(float dt);

    // Apply current values to target (respects blend mode and weight)
    void apply();

private:
    struct ResolvedTargetRun {
        size_t first_track = 0;
        size_t track_count = 0;
        Node* target = nullptr;
        std::weak_ptr<const void> target_lifetime;
    };

    Timeline* timeline_;
    std::weak_ptr<const void> timeline_lifetime_;
    Node* target_;
    std::weak_ptr<const void> target_lifetime_;
    // Single-threaded derived cache. Runs borrow nodes, lifetime tokens guard
    // destruction, and capacity never exceeds the timeline's track count.
    std::vector<ResolvedTargetRun> execution_plan_;
    uint64_t execution_plan_topology_revision_ = 0;
    uint64_t execution_plan_program_revision_ = 0;
    animation::PlaybackCursor playback_;
    BlendMode blend_mode_ = BlendMode::Override;
    float blend_weight_ = 1.0f;
    int layer_ = 0;
    TriggerCallback trigger_callback_;

    // Duration override for parameterized animations
    float duration_override_ = 0;
    float speed_multiplier_ = 1.0f;

    // Fade state (for crossfade)
    float fade_start_weight_ = 1.0f;
    float fade_target_weight_ = 1.0f;
    float fade_duration_ = 0;
    float fade_time_ = 0;

    // Fire triggers between prev_time and current time
    void fire_triggers(float from_time, float to_time, bool forward);
    static void visit_playback_interval(void* user,
                                        const animation::PlaybackInterval& interval);
    float effective_duration() const;
    float sample_time(float player_time) const;
    bool timeline_alive() const;
    bool target_alive() const;
    void invalidate_timeline();
    void invalidate_target();
    // Rebuild: O(track_count * target_lookup), stable apply: O(track_count).
    // Rebuild space is O(target_runs), where target_runs <= track_count.
    void rebuild_execution_plan(const AnimationProgram& program);
};

// ============================================================================
// AnimationController - Manages multiple timeline players with blending
// ============================================================================

class AnimationController {
public:
    AnimationController()
        : owned_allocator_(std::make_unique<ArenaAllocator>(
              default_player_arena_bytes_)),
          players_(*owned_allocator_) {}
    explicit AnimationController(ArenaAllocator& alloc) : players_(alloc) {}
    ~AnimationController() = default;

    // Register a timeline
    void add_timeline(Timeline::SharedPtr timeline);
    Timeline* get_timeline(const char* name) const;

    // Play a timeline on a target node
    TimelinePlayer* play(const char* timeline_name, Node* target);

    // Play with blending options
    TimelinePlayer* play(const char* timeline_name, Node* target,
                        BlendMode blend_mode, float blend_weight = 1.0f, int layer = 0);

    void stop(const char* timeline_name);
    void stop_all();
    void stop_on_target(Node* target);  // Stop all animations on a specific target

    // Check if playing
    bool is_playing(const char* timeline_name) const;

    /**
     * Get unfinished players currently associated with a target.
     *
     * The returned vector owns its pointer array, but each TimelinePlayer is
     * borrowed from this controller. Those pointers become invalid when the
     * controller removes finished players or is destroyed.
     */
    std::vector<TimelinePlayer*> get_players_for_target(Node* target) const;

    // Global trigger callback (receives event from any timeline)
    void set_trigger_callback(TriggerCallback callback);

    // Update all active players (sorts by layer, applies in order)
    void advance(float dt);

    // Crossfade between two timelines on the same target
    // Fades out current animations and fades in the new one over duration
    void crossfade(const char* timeline_name, Node* target,
                   float fade_duration, BlendMode blend_mode = BlendMode::Override);

private:
    static constexpr size_t default_player_arena_bytes_ = 64 * 1024;

    std::unordered_map<std::string, Timeline::SharedPtr> timelines_;
    // Declared before players_ so the borrowed arena outlives its container.
    std::unique_ptr<ArenaAllocator> owned_allocator_;
    PoolVector<std::unique_ptr<TimelinePlayer>> players_;  // Uses arena allocator
    TriggerCallback trigger_callback_;
    bool players_dirty_ = false;  // Track if players_ needs sorting

    // Remove finished players
    void cleanup_finished();

    // Sort players by layer for proper blending order
    void sort_by_layer();
};

} // namespace flex
