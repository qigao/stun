/*
 * Flex Engine - Timeline Animation System
 *
 * Keyframe-based animation with tracks, triggers, easing, and blending.
 * Optimized with arena allocator for zero-allocation animation sampling.
 */

#pragma once
#include "flex/runtime/types.h"
#include "flex/runtime/allocator.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <variant>

namespace flex {

// Forward declarations
class Node;

// ============================================================================
// Animation Value Types (using std::variant for type safety)
// ============================================================================

using AnimValue = std::variant<float, std::string, Color>;

// ============================================================================
// Loop Mode
// ============================================================================

enum class LoopMode {
    Once,       // Play once and stop
    Loop,       // Loop forever
    PingPong,   // Alternate forward/backward
};

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

struct Keyframe {
    float time = 0;           // Time in seconds
    AnimValue value;          // Value at this time
    Easing easing = Easing::linear();  // Easing function

    Keyframe() = default;
    Keyframe(float t, float v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
    Keyframe(float t, const std::string& v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
    Keyframe(float t, const Color& v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
};

// ============================================================================
// Track - Single property animation channel
// ============================================================================

class Track {
public:
    using Ptr = std::shared_ptr<Track>;

    Track(const char* property, ArenaAllocator& alloc);
    ~Track() = default;

    // Factory
    static Ptr create(const char* property, ArenaAllocator& alloc) {
        return std::make_shared<Track>(property, alloc);
    }

    // Property path (e.g., "opacity", "x", "fill.color")
    const char* property() const { return property_.c_str(); }

    // Keyframe management
    void add_keyframe(float time, float value, Easing easing = Easing::linear());
    void add_keyframe(float time, const char* value, Easing easing = Easing::linear());
    void add_keyframe(float time, const Color& value, Easing easing = Easing::linear());
    void clear_keyframes();
    size_t keyframe_count() const { return keyframes_.size(); }

    // Sample value at time
    AnimValue sample(float time) const;

    // Get duration (time of last keyframe)
    float duration() const;

private:
    std::string property_;  // Own the string to avoid dangling pointer
    PoolVector<Keyframe> keyframes_;  // Uses arena allocator

    // Find surrounding keyframes for interpolation (optimized with binary search)
    void find_keyframes(float time, const Keyframe** prev, const Keyframe** next) const;
};

// ============================================================================
// Timeline - Collection of tracks with playback control
// ============================================================================

class Timeline {
public:
    using Ptr = std::shared_ptr<Timeline>;

    Timeline(const char* name, ArenaAllocator& alloc);
    ~Timeline() = default;

    // Factory
    static Ptr create(const char* name, ArenaAllocator& alloc) {
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
    Track::Ptr add_track(const char* property);
    Track* get_track(const char* property) const;
    const PoolVector<Track::Ptr>& tracks() const { return tracks_; }

    // Trigger management
    void add_trigger(float time, const char* event);
    void clear_triggers();
    const PoolVector<Trigger>& triggers() const;
    size_t trigger_count() const { return triggers_.size(); }

    // Apply timeline values to a node at given time
    void apply(Node* target, float time) const;

private:
    std::string name_;  // Own the string to avoid dangling pointer
    PoolVector<Track::Ptr> tracks_;  // Uses arena allocator
    mutable PoolVector<Trigger> triggers_;   // Uses arena allocator (mutable for lazy sorting)
    mutable bool triggers_sorted_ = true;    // Track if triggers are sorted
    float duration_ = 0;  // 0 = auto from tracks
    LoopMode loop_mode_ = LoopMode::Once;
    float speed_ = 1.0f;
    ArenaAllocator* allocator_;  // For creating tracks

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
    Timeline* timeline() const { return timeline_; }
    Node* target() const { return target_; }

    // Playback state
    bool is_playing() const { return playing_; }
    bool is_finished() const { return finished_; }
    float current_time() const { return time_; }
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

    // Update (called each frame)
    // Returns true if still playing
    bool advance(float dt);

    // Apply current values to target (respects blend mode and weight)
    void apply();

private:
    Timeline* timeline_;
    Node* target_;
    float time_ = 0;
    float prev_time_ = 0;  // For trigger detection
    bool playing_ = false;
    bool finished_ = false;
    bool reverse_ = false;  // For ping-pong
    BlendMode blend_mode_ = BlendMode::Override;
    float blend_weight_ = 1.0f;
    int layer_ = 0;
    TriggerCallback trigger_callback_;

    // Fade state (for crossfade)
    float fade_start_weight_ = 1.0f;
    float fade_target_weight_ = 1.0f;
    float fade_duration_ = 0;
    float fade_time_ = 0;

    // Fire triggers between prev_time and current time
    void fire_triggers(float from_time, float to_time);
};

// ============================================================================
// AnimationController - Manages multiple timeline players with blending
// ============================================================================

class AnimationController {
public:
    AnimationController() : players_() {}  // Default: no allocator
    explicit AnimationController(ArenaAllocator& alloc) : players_(alloc) {}
    ~AnimationController() = default;

    // Register a timeline
    void add_timeline(Timeline::Ptr timeline);
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

    // Get all active players for a target (for manual blending)
    PoolVector<TimelinePlayer*> get_players_for_target(Node* target) const;

    // Global trigger callback (receives event from any timeline)
    void set_trigger_callback(TriggerCallback callback) { trigger_callback_ = std::move(callback); }

    // Update all active players (sorts by layer, applies in order)
    void advance(float dt);

    // Crossfade between two timelines on the same target
    // Fades out current animations and fades in the new one over duration
    void crossfade(const char* timeline_name, Node* target,
                   float fade_duration, BlendMode blend_mode = BlendMode::Override);

private:
    std::unordered_map<std::string, Timeline::Ptr> timelines_;
    PoolVector<std::unique_ptr<TimelinePlayer>> players_;  // Uses arena allocator
    TriggerCallback trigger_callback_;
    bool players_dirty_ = false;  // Track if players_ needs sorting

    // Remove finished players
    void cleanup_finished();

    // Sort players by layer for proper blending order
    void sort_by_layer();
};

} // namespace flex
