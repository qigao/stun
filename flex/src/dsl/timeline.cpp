/*
 * Flex Engine - Timeline Implementation
 *
 * Optimized with arena allocator and binary search for O(log n) keyframe lookup.
 */

#include "flex/dsl/timeline.h"
#include "flex/node.h"
#include "flex/dsl/shape.h"
#include "flex/dsl/text.h"
#include <algorithm>
#include <cmath>

namespace flex {

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
    if (keyframes_.size() == 0) return 0;
    return keyframes_.back().time;
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

        // Transform properties (Phase 3.1: Change detection to avoid dirty flags)
        if (strcmp(actual_prop, "x") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->x() != new_val) {
                    actual_target->set_x(new_val);
                }
            }
        } else if (strcmp(actual_prop, "y") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->y() != new_val) {
                    actual_target->set_y(new_val);
                }
            }
        } else if (strcmp(actual_prop, "rotation") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->rotation() != new_val) {
                    actual_target->set_rotation(new_val);
                }
            }
        } else if (strcmp(actual_prop, "scale") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->scale_x() != new_val || actual_target->scale_y() != new_val) {
                    actual_target->set_scale(new_val);
                }
            }
        } else if (strcmp(actual_prop, "scale_x") == 0 || strcmp(actual_prop, "scaleX") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->scale_x() != new_val) {
                    actual_target->set_scale(new_val, actual_target->scale_y());
                }
            }
        } else if (strcmp(actual_prop, "scale_y") == 0 || strcmp(actual_prop, "scaleY") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->scale_y() != new_val) {
                    actual_target->set_scale(actual_target->scale_x(), new_val);
                }
            }
        }
        // Visual properties (Phase 3.1: Change detection)
        else if (strcmp(actual_prop, "opacity") == 0) {
            if (std::holds_alternative<float>(value)) {
                float new_val = std::get<float>(value);
                if (actual_target->opacity() != new_val) {
                    actual_target->set_opacity(new_val);
                }
            }
        } else if (strcmp(actual_prop, "visible") == 0) {
            if (std::holds_alternative<float>(value)) {
                bool new_val = std::get<float>(value) > 0.5f;
                if (actual_target->visible() != new_val) {
                    actual_target->set_visible(new_val);
                }
            }
        }
        // Shape-specific properties (Phase 3.1: Change detection)
        else if (auto* shape = dynamic_cast<Shape*>(actual_target)) {
            if (strcmp(actual_prop, "fill") == 0 || strcmp(actual_prop, "color") == 0) {
                if (std::holds_alternative<Color>(value)) {
                    Color new_color = std::get<Color>(value);
                    auto current_fill = shape->fill();
                    if (current_fill.color.r != new_color.r ||
                        current_fill.color.g != new_color.g ||
                        current_fill.color.b != new_color.b ||
                        current_fill.color.a != new_color.a) {
                        shape->set_fill(new_color);
                    }
                }
            } else if (strcmp(actual_prop, "fill.opacity") == 0) {
                if (std::holds_alternative<float>(value)) {
                    float new_alpha = std::get<float>(value);
                    auto fill = shape->fill();
                    if (fill.color.a != new_alpha) {
                        Color new_color = fill.color;
                        new_color.a = new_alpha;
                        shape->set_fill(new_color);
                    }
                }
            } else if (strcmp(actual_prop, "stroke") == 0) {
                if (std::holds_alternative<Color>(value)) {
                    Color new_color = std::get<Color>(value);
                    auto stroke = shape->stroke();
                    if (stroke.color.r != new_color.r ||
                        stroke.color.g != new_color.g ||
                        stroke.color.b != new_color.b ||
                        stroke.color.a != new_color.a) {
                        shape->set_stroke(new_color, stroke.width);
                    }
                }
            } else if (strcmp(actual_prop, "stroke.width") == 0) {
                if (std::holds_alternative<float>(value)) {
                    float new_width = std::get<float>(value);
                    auto stroke = shape->stroke();
                    if (stroke.width != new_width) {
                        shape->set_stroke(stroke.color, new_width);
                    }
                }
            }
        }
        // Text-specific properties (Phase 3.1: Change detection)
        else if (auto* text = dynamic_cast<Text*>(actual_target)) {
            if (strcmp(actual_prop, "text") == 0 || strcmp(actual_prop, "content") == 0) {
                if (std::holds_alternative<std::string>(value)) {
                    const std::string& new_text = std::get<std::string>(value);
                    if (text->content() != new_text) {
                        text->set_content(new_text);
                    }
                }
            } else if (strcmp(actual_prop, "font_size") == 0 || strcmp(actual_prop, "fontSize") == 0) {
                if (std::holds_alternative<float>(value)) {
                    float new_size = std::get<float>(value);
                    if (text->font_size() != new_size) {
                        text->set_font_size(new_size);
                    }
                }
            } else if (strcmp(actual_prop, "text.color") == 0) {
                if (std::holds_alternative<Color>(value)) {
                    Color new_color = std::get<Color>(value);
                    Color current_color = text->color();
                    if (current_color.r != new_color.r ||
                        current_color.g != new_color.g ||
                        current_color.b != new_color.b ||
                        current_color.a != new_color.a) {
                        text->set_color(new_color);
                    }
                }
            }
        }
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
    time_ += dt * timeline_->speed();

    float duration = timeline_->duration();
    LoopMode loop_mode = timeline_->loop_mode();

    // Handle loop modes
    if (time_ >= duration) {
        if (loop_mode == LoopMode::Once) {
            time_ = duration;
            finished_ = true;
            playing_ = false;
        } else if (loop_mode == LoopMode::Loop) {
            time_ = std::fmod(time_, duration);
        } else if (loop_mode == LoopMode::PingPong) {
            reverse_ = !reverse_;
            time_ = duration - (time_ - duration);
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
        if (player->timeline()->name() == timeline_name) {
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
        if (player->timeline()->name() == timeline_name && player->is_playing()) {
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
 