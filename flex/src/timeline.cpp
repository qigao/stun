/*
 * Flex Engine - Timeline Implementation
 */

#include "flex/timeline.h"
#include "flex/node.h"
#include <algorithm>
#include <cmath>

namespace flex {

// ============================================================================
// Track Implementation
// ============================================================================

Track::Track(const std::string& property)
    : property_(property)
{
}

void Track::add_keyframe(float time, float value, Easing easing) {
    keyframes_.emplace_back(time, value, easing);
    std::sort(keyframes_.begin(), keyframes_.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.time < b.time;
    });
}

void Track::add_keyframe(float time, const std::string& value, Easing easing) {
    keyframes_.emplace_back(time, value, easing);
    std::sort(keyframes_.begin(), keyframes_.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.time < b.time;
    });
}

void Track::add_keyframe(float time, const Color& value, Easing easing) {
    keyframes_.emplace_back(time, value, easing);
    std::sort(keyframes_.begin(), keyframes_.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.time < b.time;
    });
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
    if (keyframes_.empty()) return 0;
    return keyframes_.back().time;
}

void Track::find_keyframes(float time, const Keyframe** prev, const Keyframe** next) const {
    *prev = nullptr;
    *next = nullptr;

    if (keyframes_.empty()) return;

    // Before first keyframe
    if (time <= keyframes_.front().time) {
        *prev = *next = &keyframes_.front();
        return;
    }

    // After last keyframe
    if (time >= keyframes_.back().time) {
        *prev = *next = &keyframes_.back();
        return;
    }

    // Find surrounding keyframes
    for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
        if (time >= keyframes_[i].time && time <= keyframes_[i + 1].time) {
            *prev = &keyframes_[i];
            *next = &keyframes_[i + 1];
            return;
        }
    }
}

// ============================================================================
// Timeline Implementation
// ============================================================================

Timeline::Timeline(const std::string& name)
    : name_(name)
{
}

Track::Ptr Timeline::add_track(const std::string& property) {
    auto track = Track::create(property);
    tracks_.push_back(track);
    return track;
}

Track* Timeline::get_track(const std::string& property) const {
    for (const auto& track : tracks_) {
        if (track->property() == property) {
            return track.get();
        }
    }
    return nullptr;
}

float Timeline::auto_duration() const {
    float max_duration = 0;
    for (const auto& track : tracks_) {
        max_duration = std::max(max_duration, track->duration());
    }
    if (!triggers_.empty()) {
        max_duration = std::max(max_duration, triggers_.back().time);
    }
    return max_duration;
}

void Timeline::add_trigger(float time, const std::string& event) {
    Trigger trigger;
    trigger.time = time;
    trigger.event = event;
    auto it = std::lower_bound(triggers_.begin(), triggers_.end(), trigger,
        [](const Trigger& a, const Trigger& b) { return a.time < b.time; });
    triggers_.insert(it, trigger);
}

void Timeline::clear_triggers() {
    triggers_.clear();
}

// ============================================================================
// TimelinePlayer Implementation
// ============================================================================

TimelinePlayer::TimelinePlayer(Timeline::Ptr timeline, Node* target)
    : timeline_(timeline)
    , target_(target)
{
}

float TimelinePlayer::normalized_time() const {
    float dur = timeline_->duration();
    return (dur > 0) ? time_ / dur : 0;
}

void TimelinePlayer::play() {
    playing_ = true;
    finished_ = false;
}

void TimelinePlayer::pause() {
    playing_ = false;
}

void TimelinePlayer::stop() {
    playing_ = false;
    finished_ = true;
    time_ = 0;
    prev_time_ = 0;
    reverse_ = false;
}

void TimelinePlayer::seek(float time) {
    prev_time_ = time_;
    time_ = time;
    float dur = timeline_->duration();
    if (dur > 0) {
        time_ = std::max(0.0f, std::min(time_, dur));
    }
}

void TimelinePlayer::fire_triggers(float from_time, float to_time) {
    if (!trigger_callback_) return;
    const auto& triggers = timeline_->triggers();
    if (triggers.empty()) return;

    if (from_time < to_time) {
        for (const auto& trigger : triggers) {
            if (trigger.time > from_time && trigger.time <= to_time) {
                trigger_callback_(trigger.event);
            }
        }
    } else if (from_time > to_time) {
        for (auto it = triggers.rbegin(); it != triggers.rend(); ++it) {
            if (it->time < from_time && it->time >= to_time) {
                trigger_callback_(it->event);
            }
        }
    }
}

bool TimelinePlayer::advance(float dt) {
    if (!playing_ || finished_) {
        return false;
    }

    float dur = timeline_->duration();
    float speed = timeline_->speed();
    prev_time_ = time_;

    if (reverse_) {
        time_ -= dt * speed;
    } else {
        time_ += dt * speed;
    }

    switch (timeline_->loop_mode()) {
        case LoopMode::Once:
            if (time_ >= dur) {
                fire_triggers(prev_time_, dur);
                time_ = dur;
                finished_ = true;
                playing_ = false;
            } else {
                fire_triggers(prev_time_, time_);
            }
            break;

        case LoopMode::Loop:
            if (dur > 0) {
                if (time_ >= dur) {
                    fire_triggers(prev_time_, dur);
                    while (time_ >= dur) {
                        time_ -= dur;
                    }
                    fire_triggers(0, time_);
                } else {
                    fire_triggers(prev_time_, time_);
                }
                if (time_ < 0) time_ = 0;
            }
            break;

        case LoopMode::PingPong:
            if (!reverse_ && time_ >= dur) {
                fire_triggers(prev_time_, dur);
                time_ = dur;
                reverse_ = true;
            } else if (reverse_ && time_ <= 0) {
                fire_triggers(prev_time_, 0);
                time_ = 0;
                reverse_ = false;
            } else {
                fire_triggers(prev_time_, time_);
            }
            break;
    }

    apply();
    return playing_;
}

// ============================================================================
// AnimationController Implementation
// ============================================================================

void AnimationController::add_timeline(Timeline::Ptr timeline) {
    timelines_[timeline->name()] = timeline;
}

Timeline* AnimationController::get_timeline(const std::string& name) const {
    auto it = timelines_.find(name);
    return (it != timelines_.end()) ? it->second.get() : nullptr;
}

TimelinePlayer* AnimationController::play(const std::string& timeline_name, Node* target) {
    return play(timeline_name, target, BlendMode::Override, 1.0f, 0);
}

TimelinePlayer* AnimationController::play(const std::string& timeline_name, Node* target,
                                          BlendMode blend_mode, float blend_weight, int layer) {
    auto* timeline = get_timeline(timeline_name);
    if (!timeline) return nullptr;

    auto player = std::make_unique<TimelinePlayer>(
        timelines_[timeline_name], target);
    player->set_blend_mode(blend_mode);
    player->set_blend_weight(blend_weight);
    player->set_layer(layer);
    if (trigger_callback_) {
        player->set_trigger_callback(trigger_callback_);
    }
    player->play();

    TimelinePlayer* ptr = player.get();
    players_.push_back(std::move(player));
    return ptr;
}

void AnimationController::stop(const std::string& timeline_name) {
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

bool AnimationController::is_playing(const std::string& timeline_name) const {
    for (const auto& player : players_) {
        if (player->timeline()->name() == timeline_name && player->is_playing()) {
            return true;
        }
    }
    return false;
}

std::vector<TimelinePlayer*> AnimationController::get_players_for_target(Node* target) const {
    std::vector<TimelinePlayer*> result;
    for (const auto& player : players_) {
        if (player->target() == target && player->is_playing()) {
            result.push_back(player.get());
        }
    }
    return result;
}

void AnimationController::sort_by_layer() {
    std::stable_sort(players_.begin(), players_.end(),
        [](const std::unique_ptr<TimelinePlayer>& a, const std::unique_ptr<TimelinePlayer>& b) {
            return a->layer() < b->layer();
        });
}

void AnimationController::advance(float dt) {
    sort_by_layer();
    for (auto& player : players_) {
        player->advance(dt);
    }
    cleanup_finished();
}

void AnimationController::crossfade(const std::string& timeline_name, Node* target,
                                    float fade_duration, BlendMode blend_mode) {
    // Mark existing animations on target for fade out
    // (In a full implementation, we'd track fade state per player)
    // For now, just stop existing and start new
    stop_on_target(target);

    // Start new animation with full weight
    play(timeline_name, target, blend_mode, 1.0f, 0);
    (void)fade_duration;  // TODO: Implement actual fade transition
}

void AnimationController::cleanup_finished() {
    players_.erase(
        std::remove_if(players_.begin(), players_.end(),
            [](const std::unique_ptr<TimelinePlayer>& p) {
                return p->is_finished();
            }),
        players_.end());
}

void TimelinePlayer::apply() {
    if (!target_ || blend_weight_ <= 0) return;

    for (const auto& track : timeline_->tracks()) {
        AnimValue value = track->sample(time_);

        // Apply value to target based on property name
        // This is a simplified implementation - in practice you'd have a property system
        const std::string& prop = track->property();

        // For now, only support basic numeric properties
        if (std::holds_alternative<float>(value)) {
            float float_val = std::get<float>(value);

            // Simple property mapping
            if (prop == "x") {
                target_->set_x(float_val);
            } else if (prop == "y") {
                target_->set_y(float_val);
            } else if (prop == "opacity") {
                target_->set_opacity(float_val);
            } else if (prop == "rotation") {
                target_->set_rotation(float_val);
            } else if (prop == "scaleX") {
                target_->set_scale(float_val, target_->scale_y());
            } else if (prop == "scaleY") {
                target_->set_scale(target_->scale_x(), float_val);
            }
        }
    }
}

} // namespace flex