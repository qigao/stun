#include "flex/core/timeline.h"
#include "flex/core/node.h"
#include "flex/core/shape.h"
#include "flex/core/text.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace flex {

namespace {

static bool get_numeric_property(Node* target, PropertyID pid, float& out) {
    if (!target) {
        return false;
    }

    switch (pid) {
        case PropertyID::X:
            out = target->x();
            return true;
        case PropertyID::Y:
            out = target->y();
            return true;
        case PropertyID::Rotation:
            out = target->rotation();
            return true;
        case PropertyID::Scale:
            out = target->scale_x();
            return true;
        case PropertyID::ScaleX:
            out = target->scale_x();
            return true;
        case PropertyID::ScaleY:
            out = target->scale_y();
            return true;
        case PropertyID::Opacity:
            out = target->opacity();
            return true;
        case PropertyID::Visible:
            out = target->visible() ? 1.0f : 0.0f;
            return true;
        default:
            break;
    }

    if (auto* shape = dynamic_cast<Shape*>(target)) {
        switch (pid) {
            case PropertyID::Width: {
                if (shape->geometry_type() == GeometryType::Rect) {
                    out = shape->rect().width;
                    return true;
                }
                break;
            }
            case PropertyID::Height: {
                if (shape->geometry_type() == GeometryType::Rect) {
                    out = shape->rect().height;
                    return true;
                }
                break;
            }
            case PropertyID::Radius: {
                if (shape->geometry_type() == GeometryType::Circle) {
                    out = shape->circle().radius;
                    return true;
                }
                break;
            }
            case PropertyID::StrokeWidth: {
                if (shape->has_stroke()) {
                    out = shape->stroke().width;
                    return true;
                }
                break;
            }
            case PropertyID::FillOpacity: {
                if (shape->has_fill()) {
                    out = shape->fill().color.a;
                    return true;
                }
                break;
            }
            default:
                break;
        }
    }

    if (auto* text = dynamic_cast<Text*>(target)) {
        switch (pid) {
            case PropertyID::FontSize:
                out = text->font_size();
                return true;
            default:
                break;
        }
    }

    return false;
}

static bool get_color_property(Node* target, PropertyID pid, Color& out) {
    if (!target) {
        return false;
    }

    if (auto* text = dynamic_cast<Text*>(target)) {
        if (pid == PropertyID::TextColor || pid == PropertyID::Color) {
            out = text->color();
            return true;
        }
    }

    if (auto* shape = dynamic_cast<Shape*>(target)) {
        if (pid == PropertyID::Fill || pid == PropertyID::Color) {
            if (shape->has_fill()) {
                out = shape->fill().color;
                return true;
            }
        } else if (pid == PropertyID::Stroke) {
            if (shape->has_stroke()) {
                out = shape->stroke().color;
                return true;
            }
        }
    }

    return false;
}

static Color lerp_color(const Color& a, const Color& b, float t) {
    float clamped = (std::max)(0.0f, (std::min)(1.0f, t));
    return Color(
        a.r + (b.r - a.r) * clamped,
        a.g + (b.g - a.g) * clamped,
        a.b + (b.b - a.b) * clamped,
        a.a + (b.a - a.a) * clamped
    );
}

static bool apply_blended_value(Node* target, PropertyID pid, const AnimValue& value,
                                BlendMode mode, float weight) {
    if (!target || pid == PropertyID::Unknown) {
        return false;
    }

    if (auto* fval = std::get_if<float>(&value)) {
        float base = 0.0f;
        if (!get_numeric_property(target, pid, base)) {
            if (mode == BlendMode::Override && weight >= 0.999f) {
                return target->set_animated_property(pid, value);
            }
            return false;
        }

        float result = base;
        if (mode == BlendMode::Override) {
            result = base + (*fval - base) * weight;
        } else if (mode == BlendMode::Additive) {
            result = base + (*fval) * weight;
        } else if (mode == BlendMode::Multiply) {
            float scaled = base * (*fval);
            result = base + (scaled - base) * weight;
        }
        return target->set_animated_property(pid, result);
    }

    if (auto* cval = std::get_if<Color>(&value)) {
        if (mode != BlendMode::Override) {
            if (weight >= 0.999f) {
                return target->set_animated_property(pid, value);
            }
            return false;
        }
        Color base;
        if (!get_color_property(target, pid, base)) {
            if (weight >= 0.999f) {
                return target->set_animated_property(pid, value);
            }
            return false;
        }
        Color blended = lerp_color(base, *cval, weight);
        return target->set_animated_property(pid, blended);
    }

    if (auto* sval = std::get_if<std::string>(&value)) {
        if (mode == BlendMode::Override && weight >= 0.999f) {
            return target->set_animated_property(pid, *sval);
        }
    }

    return false;
}

} // namespace

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

void Track::set_numeric_expression(const std::string& expression) {
    numeric_expression_ = std::make_shared<animation::NumericExpression>(expression);
}

void Track::clear_numeric_expression() {
    numeric_expression_.reset();
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
        if (numeric_expression_) {
            return AnimValue(numeric_expression_->sample(
                {time, t, prev_val, next_val}));
        }
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

Track::SharedPtr Timeline::add_track(const char* property) {
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
        max_duration = (std::max)(max_duration, track->duration());
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
    : timeline_(timeline), target_(target),
      target_lifetime_(target ? target->lifetime_token() : std::weak_ptr<const void>{})
{
}

bool TimelinePlayer::target_alive() const {
    return target_ && !target_lifetime_.expired();
}

void TimelinePlayer::invalidate_target() {
    target_ = nullptr;
    target_lifetime_.reset();
    playback_.stop();
}

float TimelinePlayer::effective_duration() const {
    if (!timeline_) {
        return 0.0f;
    }
    return duration_override_ > 0 ? duration_override_ : timeline_->duration();
}

float TimelinePlayer::sample_time(float player_time) const {
    if (!timeline_) {
        return 0.0f;
    }

    const float timeline_duration = timeline_->duration();
    const float player_duration = effective_duration();
    if (duration_override_ > 0 && timeline_duration > 0 && player_duration > 0) {
        return player_time * timeline_duration / player_duration;
    }
    return player_time;
}

float TimelinePlayer::normalized_time() const {
    float dur = effective_duration();
    return dur > 0 ? playback_.time() / dur : 0;
}

void TimelinePlayer::play() {
    if (!target_alive()) {
        invalidate_target();
        return;
    }
    playback_.play();

    // Apply initial frame immediately to avoid jumping
    apply();
}

void TimelinePlayer::pause() {
    playback_.pause();
}

void TimelinePlayer::stop() {
    playback_.stop();
}

void TimelinePlayer::seek(float time) {
    playback_.seek(time, effective_duration());
}

bool TimelinePlayer::advance(float dt) {
    if (!target_alive()) {
        invalidate_target();
        return false;
    }
    if (!timeline_ || !playback_.playing() || playback_.finished()) return false;

    const auto result = playback_.advance(
        dt, effective_duration(), timeline_->speed() * speed_multiplier_,
        timeline_->loop_mode(), &TimelinePlayer::visit_playback_interval, this);
    if (!result.valid) {
        return false;
    }

    // Update fade weight
    if (fade_duration_ > 0 && fade_time_ < fade_duration_) {
        fade_time_ += dt;
        float t = (std::min)(fade_time_ / fade_duration_, 1.0f);

        // Linear interpolation for now (can be improved with easing)
        blend_weight_ = fade_start_weight_ + (fade_target_weight_ - fade_start_weight_) * t;

        if (fade_time_ >= fade_duration_) {
            blend_weight_ = fade_target_weight_;
            fade_duration_ = 0;  // Mark fade as complete
        }
    }

    return result.active;
}

void TimelinePlayer::apply() {
    if (!target_alive()) {
        invalidate_target();
        return;
    }
    // Phase 3.1: Skip if not playing, finished, or time unchanged
    if ((!playback_.playing() && !playback_.finished()) ||
        playback_.time() == playback_.previous_time()) return;

    float weight = blend_weight_;
    if (weight <= 0.0f) {
        return;
    }

    if (blend_mode_ == BlendMode::Override && weight >= 0.999f) {
        timeline_->apply(target_, sample_time(playback_.time()));
        return;
    }

    float sampled_time = sample_time(playback_.time());
    for (auto& track : timeline_->tracks()) {
        auto value = track->sample(sampled_time);
        const char* prop = track->property();
        Node* actual_target = target_;
        const char* actual_prop = prop;

        if (prop[0] == '#') {
            std::string prop_str(prop);
            size_t slash = prop_str.find('/');
            if (slash != std::string::npos) {
                std::string id = prop_str.substr(1, slash - 1);
                actual_target = target_ ? target_->find(id) : nullptr;
                actual_prop = prop + slash + 1;
            }
        }

        if (!actual_target) {
            continue;
        }

        PropertyID pid = get_property_id(actual_prop);
        apply_blended_value(actual_target, pid, value, blend_mode_, weight);
    }
}

void TimelinePlayer::visit_playback_interval(
    void* user, const animation::PlaybackInterval& interval) {
    auto* player = static_cast<TimelinePlayer*>(user);
    player->fire_triggers(player->sample_time(interval.from),
                          player->sample_time(interval.to), interval.forward);
}

void TimelinePlayer::fire_triggers(float from_time, float to_time, bool forward) {
    const auto& triggers = timeline_->triggers();

    if (forward) {
        auto start = std::lower_bound(triggers.begin(), triggers.end(), from_time,
            [](const Trigger& trigger, float time) { return trigger.time < time; });
        for (auto it = start; it != triggers.end() && it->time < to_time; ++it) {
            if (trigger_callback_) trigger_callback_(it->event);
        }
        return;
    }

    auto end = std::upper_bound(triggers.begin(), triggers.end(), from_time,
        [](float time, const Trigger& trigger) { return time < trigger.time; });
    while (end != triggers.begin()) {
        --end;
        if (end->time <= to_time) {
            break;
        }
        if (trigger_callback_) trigger_callback_(end->event);
    }
}

// ============================================================================
// AnimationController Implementation
// ============================================================================

void AnimationController::add_timeline(Timeline::SharedPtr timeline) {
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
    if (trigger_callback_) {
        player->set_trigger_callback(trigger_callback_);
    }
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
        player->advance(dt);
        player->apply();
    }

    cleanup_finished();
}

void AnimationController::set_trigger_callback(TriggerCallback callback) {
    trigger_callback_ = std::move(callback);
    for (auto& player : players_) {
        player->set_trigger_callback(trigger_callback_);
    }
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
