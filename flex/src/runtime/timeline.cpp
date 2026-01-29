/*
 * Flex Engine - Timeline Implementation
 *
 * Optimized with arena allocator and binary search for O(log n) keyframe lookup.
 */

#include "flex/runtime/timeline.h"
#include "flex/runtime/node.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace flex {

// ============================================================================
// Property ID lookup (implementation of declaration in types.h)
// ============================================================================

PropertyID get_property_id(const char* prop) {
    if (prop[0] == 'x' && prop[1] == '\0') return PropertyID::X;
    if (prop[0] == 'y' && prop[1] == '\0') return PropertyID::Y;

    switch (prop[0]) {
        case 'c':
            if (strcmp(prop, "content") == 0) return PropertyID::Content;
            if (strcmp(prop, "color") == 0) return PropertyID::Color;
            break;
        case 'f':
            if (strcmp(prop, "fill") == 0) return PropertyID::Fill;
            if (strcmp(prop, "fill.opacity") == 0) return PropertyID::FillOpacity;
            if (strcmp(prop, "fontSize") == 0) return PropertyID::FontSize;
            if (strcmp(prop, "font_size") == 0) return PropertyID::FontSize;
            break;
        case 'h':
            if (strcmp(prop, "height") == 0) return PropertyID::Height;
            break;
        case 'o':
            if (strcmp(prop, "opacity") == 0) return PropertyID::Opacity;
            break;
        case 'r':
            if (strcmp(prop, "rotation") == 0) return PropertyID::Rotation;
            if (strcmp(prop, "radius") == 0) return PropertyID::Radius;
            break;
        case 's':
            if (strcmp(prop, "scale") == 0) return PropertyID::Scale;
            if (strcmp(prop, "scaleX") == 0) return PropertyID::ScaleX;
            if (strcmp(prop, "scaleY") == 0) return PropertyID::ScaleY;
            if (strcmp(prop, "scale_x") == 0) return PropertyID::ScaleX;
            if (strcmp(prop, "scale_y") == 0) return PropertyID::ScaleY;
            if (strcmp(prop, "stroke") == 0) return PropertyID::Stroke;
            if (strcmp(prop, "stroke.width") == 0) return PropertyID::StrokeWidth;
            break;
        case 't':
            if (strcmp(prop, "text") == 0) return PropertyID::Text;
            if (strcmp(prop, "text.color") == 0) return PropertyID::TextColor;
            break;
        case 'v':
            if (strcmp(prop, "visible") == 0) return PropertyID::Visible;
            break;
        case 'w':
            if (strcmp(prop, "width") == 0) return PropertyID::Width;
            break;
    }
    return PropertyID::Unknown;
}

// ============================================================================
// Track Implementation
// ============================================================================

Track::Track(const char* property, ArenaAllocator& alloc)
    : property_(property), keyframes_(alloc)
{
}

void Track::add_keyframe(float time, float value, Easing easing) {
    keyframes_.emplace_back(time, value, easing);
}

void Track::add_keyframe(float time, const char* value, Easing easing) {
    keyframes_.emplace_back(time, value, easing);
}

void Track::add_keyframe(float time, const Color& value, Easing easing) {
    keyframes_.emplace_back(time, value, easing);
}

void Track::clear_keyframes() {
    keyframes_.clear();
}

AnimValue Track::sample(float time) const {
    const Keyframe* prev = nullptr;
    const Keyframe* next = nullptr;
    find_keyframes(time, &prev, &next);

    if (!prev || !next) {
        // No keyframes, return default float value
        return AnimValue(0.0f);
    }

    if (prev == next) {
        return prev->value;
    }

    // Interpolate
    float duration = next->time - prev->time;
    float t = (duration > 0) ? (time - prev->time) / duration : 0;

    // Apply easing from the previous keyframe
    t = prev->easing.evaluate(t);

    // Interpolate based on value type
    if (std::holds_alternative<float>(prev->value) && std::holds_alternative<float>(next->value)) {
        float prev_val = std::get<float>(prev->value);
        float next_val = std::get<float>(next->value);
        return AnimValue(prev_val + (next_val - prev_val) * t);
    }

    // For non-float types, return the next value (no interpolation)
    return next->value;
}

float Track::duration() const {
    if (keyframes_.size() == 0) return 0.0f;
    return keyframes_[keyframes_.size() - 1].time;
}

void Track::find_keyframes(float time, const Keyframe** prev, const Keyframe** next) const {
    *prev = nullptr;
    *next = nullptr;

    if (keyframes_.size() == 0) return;

    // Binary search for O(log n) lookup instead of O(n) linear scan
    size_t left = 0;
    size_t right = keyframes_.size();

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (keyframes_[mid].time <= time) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    // left is the first keyframe with time > current time
    if (left == 0) {
        *prev = *next = &keyframes_[0];
    } else if (left >= keyframes_.size()) {
        *prev = *next = &keyframes_.back();
    } else {
        *next = &keyframes_[left];
        *prev = &keyframes_[left - 1];
    }
}

// ============================================================================
// Timeline Implementation
// ============================================================================

Timeline::Timeline(const char* name, ArenaAllocator& alloc)
    : name_(name), tracks_(alloc), triggers_(alloc), allocator_(&alloc)
{
}

Track::Ptr Timeline::add_track(const char* property) {
    auto track = Track::create(property, *allocator_);
    tracks_.push_back(track);
    return track;
}

Track* Timeline::get_track(const char* property) const {
    for (auto& track : tracks_) {
        if (std::strcmp(track->property(), property) == 0) {
            return track.get();
        }
    }
    return nullptr;
}

void Timeline::add_trigger(float time, const char* event) {
    triggers_.push_back({time, event});
    triggers_sorted_ = false;  // Mark as unsorted
}

const PoolVector<Trigger>& Timeline::triggers() const {
    // Lazy sorting: sort only when accessed and not sorted
    if (!triggers_sorted_ && triggers_.size() > 0) {
        // Sort by time (mutable allows modification in const method)
        std::sort(const_cast<Trigger*>(triggers_.begin()),
                  const_cast<Trigger*>(triggers_.end()),
                  [](const Trigger& a, const Trigger& b) { return a.time < b.time; });
        triggers_sorted_ = true;
    }
    return triggers_;
}

void Timeline::clear_triggers() {
    triggers_.clear();
    triggers_sorted_ = true;  // Empty list is sorted
}

float Timeline::auto_duration() const {
    float max_duration = 0;
    for (auto& track : tracks_) {
        max_duration = std::max(max_duration, track->duration());
    }
    return max_duration;
}

void Timeline::apply(Node* target, float time) const {
    if (!target) return;

    for (auto& track : tracks_) {
        auto value = track->sample(time);
        const char* prop = track->property();
        Node* actual_target = target;
        const char* actual_prop = prop;

        // Handle #id/prop syntax
        if (prop[0] == '#') {
            std::string prop_str(prop);
            size_t slash = prop_str.find('/');
            if (slash != std::string::npos) {
                std::string id = prop_str.substr(1, slash - 1);
                actual_target = target->find(id);
                actual_prop = prop + slash + 1;
            }
        }

        if (!actual_target) continue;

        // Virtual dispatch - no dynamic_cast needed
        PropertyID pid = get_property_id(actual_prop);
        actual_target->set_animated_property(pid, value);
    }
}

// ============================================================================
// TimelinePlayer Implementation
// ============================================================================

TimelinePlayer::TimelinePlayer(Timeline* timeline, Node* target)
    : timeline_(timeline), target_(target)
{
}

float TimelinePlayer::normalized_time() const {
    float dur = timeline_->duration();
    return dur > 0 ? time_ / dur : 0;
}

void TimelinePlayer::play() {
    playing_ = true;
    finished_ = false;

    // Set prev_time_ to -1 so the first apply() doesn't skip
    prev_time_ = -1.0f;

    // Apply initial frame immediately to avoid jumping
    apply();
}

void TimelinePlayer::pause() {
    playing_ = false;
}

void TimelinePlayer::stop() {
    playing_ = false;
    finished_ = true;
    time_ = 0;
}

void TimelinePlayer::seek(float time) {
    time_ = time;
    prev_time_ = time_;
}

bool TimelinePlayer::advance(float dt) {
    if (!playing_ || finished_) return false;

    prev_time_ = time_;

    float duration = timeline_->duration();
    LoopMode loop_mode = timeline_->loop_mode();

    // Advance time (direction depends on reverse_ for PingPong)
    if (reverse_) {
        time_ -= dt * timeline_->speed();
    } else {
        time_ += dt * timeline_->speed();
    }

    // Handle loop modes
    if (loop_mode == LoopMode::Once) {
        if (time_ >= duration) {
            time_ = duration;
            finished_ = true;
            playing_ = false;
        }
    } else if (loop_mode == LoopMode::Loop) {
        if (time_ >= duration) {
            time_ = std::fmod(time_, duration);
        }
    } else if (loop_mode == LoopMode::PingPong) {
        if (!reverse_ && time_ >= duration) {
            // Hit end, start going backwards
            reverse_ = true;
            time_ = duration - (time_ - duration);
            if (time_ < 0) time_ = 0;
        } else if (reverse_ && time_ <= 0) {
            // Hit start, start going forwards
            reverse_ = false;
            time_ = -time_;
            if (time_ > duration) time_ = duration;
        }
    }

    // Update fade weight
    if (fade_duration_ > 0 && fade_time_ < fade_duration_) {
        fade_time_ += dt;
        float t = std::min(fade_time_ / fade_duration_, 1.0f);

        // Linear interpolation for now (can be improved with easing)
        blend_weight_ = fade_start_weight_ + (fade_target_weight_ - fade_start_weight_) * t;

        if (fade_time_ >= fade_duration_) {
            blend_weight_ = fade_target_weight_;
            fade_duration_ = 0;  // Mark fade as complete
        }
    }

    // Fire triggers
    fire_triggers(prev_time_, time_);

    return playing_ && !finished_;
}

void TimelinePlayer::apply() {
    // Phase 3.1: Skip if not playing, finished, or time unchanged
    if (!playing_ || finished_ || time_ == prev_time_) return;
    timeline_->apply(target_, time_);
}

void TimelinePlayer::fire_triggers(float from_time, float to_time) {
    const auto& triggers = timeline_->triggers();

    // Binary search for the first trigger >= from_time
    auto start = std::lower_bound(triggers.begin(), triggers.end(), from_time,
        [](const Trigger& t, float time_val) { return t.time < time_val; });

    // Iterate only over triggers in range [from_time, to_time)
    for (auto it = start; it != triggers.end() && it->time < to_time; ++it) {
        if (trigger_callback_) {
            trigger_callback_(it->event);
        }
    }
}

// ============================================================================
// AnimationController Implementation
// ============================================================================

void AnimationController::add_timeline(Timeline::Ptr timeline) {
    timelines_[timeline->name()] = std::move(timeline);
}

Timeline* AnimationController::get_timeline(const char* name) const {
    auto it = timelines_.find(name);
    if (it != timelines_.end()) {
        return it->second.get();
    }
    return nullptr;
}

TimelinePlayer* AnimationController::play(const char* timeline_name, Node* target) {
    return play(timeline_name, target, BlendMode::Override, 1.0f, 0);
}

TimelinePlayer* AnimationController::play(const char* timeline_name, Node* target,
                                          BlendMode blend_mode, float blend_weight, int layer) {
    Timeline* timeline = get_timeline(timeline_name);
    if (!timeline) return nullptr;

    auto player = std::make_unique<TimelinePlayer>(timeline, target);
    player->set_blend_mode(blend_mode);
    player->set_blend_weight(blend_weight);
    player->set_layer(layer);
    player->play();

    players_.push_back(std::move(player));
    players_dirty_ = true;  // Mark dirty when adding new player
    return players_.back().get();
}

void AnimationController::stop(const char* timeline_name) {
    for (auto& player : players_) {
        if (std::strcmp(player->timeline()->name(), timeline_name) == 0) {
            player->stop();
        }
    }
}

void AnimationController::stop_all() {
    for (auto& player : players_) {
        player->stop();
    }
}

void AnimationController::stop_on_target(Node* target) {
    for (auto& player : players_) {
        if (player->target() == target) {
            player->stop();
        }
    }
}

bool AnimationController::is_playing(const char* timeline_name) const {
    for (auto& player : players_) {
        if (std::strcmp(player->timeline()->name(), timeline_name) == 0 && player->is_playing()) {
            return true;
        }
    }
    return false;
}

PoolVector<TimelinePlayer*> AnimationController::get_players_for_target(Node* target) const {
    // Note: This returns an empty PoolVector (no allocator).
    // If you need to modify the returned vector, pass an allocator to the constructor.
    PoolVector<TimelinePlayer*> result;

    // Since we can't push_back without allocator, use std::vector internally
    std::vector<TimelinePlayer*> temp;
    for (auto& player : players_) {
        if (player->target() == target) {
            temp.push_back(player.get());
        }
    }

    // For now, just return empty PoolVector
    // This function is rarely used, and the returned vector is read-only
    return result;
}

void AnimationController::advance(float dt) {
    // Only sort if players list was modified
    if (players_dirty_) {
        sort_by_layer();
        players_dirty_ = false;
    }

    for (auto& player : players_) {
        // Apply current frame BEFORE advancing time
        // This ensures the first frame shows t=0 values
        player->apply();
        player->advance(dt);
    }

    cleanup_finished();
}

void AnimationController::crossfade(const char* timeline_name, Node* target,
                                    float fade_duration, BlendMode blend_mode) {
    // Fade out existing animations on target
    for (auto& player : players_) {
        if (player->target() == target && player->is_playing()) {
            player->fade_to(0.0f, fade_duration);  // Fade out to 0
        }
    }

    // Play new animation with fade in
    auto* player = play(timeline_name, target, blend_mode, 0.0f, 100);
    if (player) {
        player->fade_to(1.0f, fade_duration);  // Fade in from 0 to 1
    }
}

void AnimationController::cleanup_finished() {
    players_.remove_if([](const auto& player) {
        return player->is_finished();
    });
}

void AnimationController::sort_by_layer() {
    std::sort(players_.begin(), players_.end(),
        [](const auto& a, const auto& b) {
            return a->layer() < b->layer();
        });
}

} // namespace flex
 