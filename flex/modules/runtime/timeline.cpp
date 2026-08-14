#include "flex/core/timeline.h"
#include "animation_sampling.h"
#include "flex/core/node.h"
#include "flex/core/shape.h"
#include "flex/core/text.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace flex {

struct Track::ProgramRevisionState {
    uint64_t value = 1;
};

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

static bool get_vec2_property(Node* target, PropertyID pid, Vec2& out) {
    if (!target || pid != PropertyID::Position) {
        return false;
    }
    out = {target->x(), target->y()};
    return true;
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

static Vec2 lerp_vec2(const Vec2& a, const Vec2& b, float t) {
    const float clamped = (std::max)(0.0f, (std::min)(1.0f, t));
    return {a.x + (b.x - a.x) * clamped,
            a.y + (b.y - a.y) * clamped};
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

    if (auto* vval = std::get_if<Vec2>(&value)) {
        Vec2 base;
        if (!get_vec2_property(target, pid, base)) {
            return false;
        }

        Vec2 result = base;
        if (mode == BlendMode::Override) {
            result = lerp_vec2(base, *vval, weight);
        } else if (mode == BlendMode::Additive) {
            result = base + (*vval * weight);
        } else if (mode == BlendMode::Multiply) {
            const Vec2 scaled{base.x * vval->x, base.y * vval->y};
            result = lerp_vec2(base, scaled, weight);
        }
        return target->set_animated_property(pid, result);
    }

    if (auto* sval = std::get_if<std::string>(&value)) {
        if (mode == BlendMode::Override && weight >= 0.999f) {
            return target->set_animated_property(pid, *sval);
        }
    }

    return false;
}

AnimationProgram::ValueKind classify_program_track(const Keyframe* keyframes,
                                                   size_t keyframe_count) {
    if (keyframe_count == 0) {
        return AnimationProgram::ValueKind::Empty;
    }

    AnimationProgram::ValueKind kind = AnimationProgram::ValueKind::Generic;
    if (std::holds_alternative<float>(keyframes[0].value)) {
        kind = AnimationProgram::ValueKind::Scalar;
    } else if (std::holds_alternative<Vec2>(keyframes[0].value)) {
        kind = AnimationProgram::ValueKind::Vec2;
    } else if (std::holds_alternative<Color>(keyframes[0].value)) {
        kind = AnimationProgram::ValueKind::Color;
    }

    if (kind == AnimationProgram::ValueKind::Generic) {
        return kind;
    }

    const size_t variant_index = keyframes[0].value.index();
    for (size_t index = 1; index < keyframe_count; ++index) {
        if (keyframes[index].value.index() != variant_index) {
            return AnimationProgram::ValueKind::Generic;
        }
    }
    return kind;
}

void checked_add_keyframes(size_t count, size_t maximum, size_t* total,
                           const char* message) {
    if (*total > maximum || count > maximum - *total) {
        throw std::length_error(message);
    }
    *total += count;
}

} // namespace

// ============================================================================
// Track Implementation
// ============================================================================

Track::Track(const char* property, ArenaAllocator& alloc)
    : property_(property ? property : ""), keyframes_(alloc)
{
    const char* property_name = property_.c_str();
    if (!property_.empty() && property_[0] == '#') {
        const size_t slash = property_.find('/');
        if (slash != std::string::npos) {
            has_target_selector_ = true;
            target_id_ = property_.substr(1, slash - 1);
            property_name = property_.c_str() + slash + 1;
        }
    }
    property_id_ = get_property_id(property_name);
}

void Track::bind_program_revision(
    const std::shared_ptr<ProgramRevisionState>& revision) {
    program_revision_ = revision;
}

void Track::mark_program_dirty() {
    auto revision = program_revision_.lock();
    if (!revision) {
        return;
    }
    if (revision->value == (std::numeric_limits<uint64_t>::max)()) {
        throw std::overflow_error("Timeline animation program revision overflow");
    }
    ++revision->value;
}

void Track::add_keyframe(float time, float value, Easing easing) {
    mark_program_dirty();
    keyframes_.emplace_back(time, value, easing);
}

void Track::add_keyframe(float time, const char* value, Easing easing) {
    mark_program_dirty();
    keyframes_.emplace_back(time, value, easing);
}

void Track::add_keyframe(float time, const Color& value, Easing easing) {
    mark_program_dirty();
    keyframes_.emplace_back(time, value, easing);
}

void Track::add_keyframe(float time, const Vec2& value, Easing easing) {
    mark_program_dirty();
    keyframes_.emplace_back(time, value, easing);
}

void Track::add_spatial_keyframe(float time, const Vec2& value,
                                 const Vec2& in_tangent,
                                 const Vec2& out_tangent, Easing easing) {
    mark_program_dirty();
    keyframes_.emplace_back(time, value, in_tangent, out_tangent, easing);
}

void Track::clear_keyframes() {
    if (keyframes_.size() == 0) {
        return;
    }
    mark_program_dirty();
    keyframes_.clear();
}

void Track::set_numeric_expression(const std::string& expression) {
    auto compiled = std::make_shared<animation::NumericExpression>(expression);
    mark_program_dirty();
    numeric_expression_ = std::move(compiled);
}

void Track::clear_numeric_expression() {
    if (!numeric_expression_) {
        return;
    }
    mark_program_dirty();
    numeric_expression_.reset();
}

void Track::set_spatial_interpolation(SpatialInterpolation interpolation) {
    if (spatial_interpolation_ == interpolation) {
        return;
    }
    mark_program_dirty();
    spatial_interpolation_ = interpolation;
}

AnimValue Track::sample(float time) const {
    return sample_keyframes(keyframes_.data(), keyframes_.size(),
                            numeric_expression_.get(), spatial_interpolation_,
                            time);
}

float Track::duration() const {
    if (keyframes_.size() == 0) return 0.0f;
    return keyframes_[keyframes_.size() - 1].time;
}

// ============================================================================
// Timeline Implementation
// ============================================================================

Timeline::Timeline(const char* name, ArenaAllocator& alloc)
    : program_revision_(std::make_shared<Track::ProgramRevisionState>()),
      name_(name), tracks_(alloc), triggers_(alloc), allocator_(&alloc)
{
}

Track::SharedPtr Timeline::add_track(const char* property) {
    auto track = Track::create(property, *allocator_);
    track->bind_program_revision(program_revision_);
    track->mark_program_dirty();
    tracks_.push_back(track);
    return track;
}

const AnimationProgram& Timeline::program() const {
    const uint64_t revision = program_revision_->value;
    if (compiled_program_ &&
        compiled_program_->source_revision() == revision) {
        return *compiled_program_;
    }

    std::vector<AnimationProgram::TrackOperation> operations;
    std::vector<AnimationProgram::ValueKind> kinds;
    AnimationProgram::Storage storage;
    operations.reserve(tracks_.size());
    kinds.reserve(tracks_.size());
    size_t total_keyframes = 0;
    size_t scalar_keyframes = 0;
    size_t vec2_keyframes = 0;
    size_t color_keyframes = 0;
    size_t generic_keyframes = 0;
    for (const auto& track : tracks_) {
        const size_t count = track->keyframes_.size();
        checked_add_keyframes(
            count, (std::numeric_limits<size_t>::max)(), &total_keyframes,
            "Timeline compiled keyframe count exceeds size_t capacity");
        const auto kind = classify_program_track(track->keyframes_.data(), count);
        kinds.push_back(kind);
        switch (kind) {
        case AnimationProgram::ValueKind::Empty:
            break;
        case AnimationProgram::ValueKind::Scalar:
            checked_add_keyframes(
                count, storage.scalar_keyframes.max_size(), &scalar_keyframes,
                "Timeline compiled scalar keyframe count exceeds vector capacity");
            break;
        case AnimationProgram::ValueKind::Vec2:
            checked_add_keyframes(
                count, storage.vec2_keyframes.max_size(), &vec2_keyframes,
                "Timeline compiled Vec2 keyframe count exceeds vector capacity");
            break;
        case AnimationProgram::ValueKind::Color:
            checked_add_keyframes(
                count, storage.color_keyframes.max_size(), &color_keyframes,
                "Timeline compiled color keyframe count exceeds vector capacity");
            break;
        case AnimationProgram::ValueKind::Generic:
            checked_add_keyframes(
                count, storage.generic_keyframes.max_size(),
                &generic_keyframes,
                "Timeline compiled generic keyframe count exceeds vector capacity");
            break;
        }
    }
    storage.keyframe_count = total_keyframes;
    storage.scalar_keyframes.reserve(scalar_keyframes);
    storage.vec2_keyframes.reserve(vec2_keyframes);
    storage.color_keyframes.reserve(color_keyframes);
    storage.vec2_tangents.reserve(vec2_keyframes);
    storage.vec2_tangent_flags.reserve(vec2_keyframes);
    storage.generic_keyframes.reserve(generic_keyframes);

    size_t logical_offset = 0;
    for (size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
        const auto& track = tracks_[track_index];
        AnimationProgram::TrackOperation operation;
        operation.keyframe_offset = logical_offset;
        operation.keyframe_count = track->keyframes_.size();
        operation.value_kind = kinds[track_index];
        operation.property_id = track->property_id_;
        operation.spatial_interpolation = track->spatial_interpolation_;
        operation.numeric_expression = track->numeric_expression_;
        operation.target_id = track->target_id_;
        operation.has_target_selector = track->has_target_selector_;
        logical_offset += operation.keyframe_count;

        switch (operation.value_kind) {
        case AnimationProgram::ValueKind::Empty:
            break;
        case AnimationProgram::ValueKind::Scalar:
            operation.storage_value_offset = storage.scalar_keyframes.size();
            for (const auto& keyframe : track->keyframes_) {
                storage.scalar_keyframes.push_back(
                    {keyframe.time, std::get<float>(keyframe.value),
                     keyframe.easing});
            }
            break;
        case AnimationProgram::ValueKind::Vec2:
            operation.storage_value_offset = storage.vec2_keyframes.size();
            for (const auto& keyframe : track->keyframes_) {
                storage.vec2_keyframes.push_back(
                    {keyframe.time, std::get<Vec2>(keyframe.value),
                     keyframe.easing});
                storage.vec2_tangents.push_back(keyframe.spatial_tangents.value_or(
                    Keyframe::SpatialTangents{}));
                storage.vec2_tangent_flags.push_back(
                    keyframe.spatial_tangents.has_value() ? uint8_t{1}
                                                          : uint8_t{0});
            }
            break;
        case AnimationProgram::ValueKind::Color:
            operation.storage_value_offset = storage.color_keyframes.size();
            for (const auto& keyframe : track->keyframes_) {
                storage.color_keyframes.push_back(
                    {keyframe.time, std::get<Color>(keyframe.value),
                     keyframe.easing});
            }
            break;
        case AnimationProgram::ValueKind::Generic:
            operation.storage_value_offset = storage.generic_keyframes.size();
            storage.generic_keyframes.insert(storage.generic_keyframes.end(),
                                             track->keyframes_.begin(),
                                             track->keyframes_.end());
            break;
        }
        operations.push_back(std::move(operation));
    }

    compiled_program_.reset(new AnimationProgram(
        revision, std::move(operations), std::move(storage)));
    return *compiled_program_;
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

    const AnimationProgram& compiled = program();
    const auto& operations = compiled.operations();
    const std::string* cached_target_id = nullptr;
    Node* cached_target = nullptr;
    for (size_t index = 0; index < operations.size(); ++index) {
        const auto& operation = operations[index];
        auto value = compiled.sample_unchecked(index, time);
        Node* actual_target = target;
        if (operation.has_target_selector) {
            if (!cached_target_id || *cached_target_id != operation.target_id) {
                cached_target_id = &operation.target_id;
                cached_target = target->find(operation.target_id);
            }
            actual_target = cached_target;
        }

        if (!actual_target) continue;

        // Virtual dispatch - no dynamic_cast needed
        actual_target->set_animated_property(operation.property_id, value);
    }
}

// ============================================================================
// TimelinePlayer Implementation
// ============================================================================

TimelinePlayer::TimelinePlayer(Timeline* timeline, Node* target)
    : timeline_(timeline),
      timeline_lifetime_(timeline ? timeline->lifetime_token()
                                  : std::weak_ptr<const void>{}),
      target_(target),
      target_lifetime_(target ? target->lifetime_token() : std::weak_ptr<const void>{})
{
}

bool TimelinePlayer::timeline_alive() const {
    return timeline_ && !timeline_lifetime_.expired();
}

bool TimelinePlayer::target_alive() const {
    return target_ && !target_lifetime_.expired();
}

void TimelinePlayer::invalidate_timeline() {
    timeline_ = nullptr;
    timeline_lifetime_.reset();
    execution_plan_.clear();
    execution_plan_topology_revision_ = 0;
    execution_plan_program_revision_ = 0;
    playback_.stop();
}

void TimelinePlayer::invalidate_target() {
    target_ = nullptr;
    target_lifetime_.reset();
    execution_plan_.clear();
    execution_plan_topology_revision_ = 0;
    execution_plan_program_revision_ = 0;
    playback_.stop();
}

void TimelinePlayer::rebuild_execution_plan(const AnimationProgram& program) {
    execution_plan_.clear();
    execution_plan_topology_revision_ = 0;
    execution_plan_program_revision_ = 0;
    if (!timeline_alive() || !target_alive()) {
        return;
    }

    const auto& operations = program.operations();
    execution_plan_.reserve(operations.size());

    const std::string* cached_target_id = nullptr;
    Node* resolved_target = nullptr;
    for (size_t index = 0; index < operations.size(); ++index) {
        const auto& operation = operations[index];
        Node* track_target = target_;
        if (operation.has_target_selector) {
            if (!cached_target_id || *cached_target_id != operation.target_id) {
                cached_target_id = &operation.target_id;
                resolved_target = target_->find(operation.target_id);
            }
            track_target = resolved_target;
        }

        if (!execution_plan_.empty() &&
            execution_plan_.back().target == track_target) {
            ++execution_plan_.back().track_count;
            continue;
        }

        ResolvedTargetRun run;
        run.first_track = index;
        run.track_count = 1;
        run.target = track_target;
        if (track_target) {
            run.target_lifetime = track_target->lifetime_token();
        }
        execution_plan_.push_back(std::move(run));
    }

    execution_plan_topology_revision_ = target_->topology_revision_;
    execution_plan_program_revision_ = program.source_revision();
}

float TimelinePlayer::effective_duration() const {
    if (!timeline_alive()) {
        return 0.0f;
    }
    return duration_override_ > 0 ? duration_override_ : timeline_->duration();
}

float TimelinePlayer::sample_time(float player_time) const {
    if (!timeline_alive()) {
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
    if (!timeline_alive()) {
        invalidate_timeline();
        return;
    }
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
    if (!timeline_alive()) {
        invalidate_timeline();
        return false;
    }
    if (!target_alive()) {
        invalidate_target();
        return false;
    }
    if (!playback_.playing() || playback_.finished()) return false;

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
    if (!timeline_alive()) {
        invalidate_timeline();
        return;
    }
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

    const AnimationProgram& program = timeline_->program();
    if (execution_plan_topology_revision_ != target_->topology_revision_ ||
        execution_plan_program_revision_ != program.source_revision()) {
        rebuild_execution_plan(program);
    }

    const auto& operations = program.operations();
    const float sampled_time = sample_time(playback_.time());
    const bool direct_override =
        blend_mode_ == BlendMode::Override && weight >= 0.999f;
    for (const auto& run : execution_plan_) {
        if (!run.target || run.target_lifetime.expired()) {
            continue;
        }

        const size_t end = run.first_track + run.track_count;
        for (size_t index = run.first_track; index < end; ++index) {
            const auto& operation = operations[index];
            auto value = program.sample_unchecked(index, sampled_time);
            if (direct_override) {
                run.target->set_animated_property(operation.property_id, value);
            } else {
                apply_blended_value(run.target, operation.property_id, value,
                                    blend_mode_, weight);
            }
        }
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
        auto* timeline = player->timeline();
        if (timeline && std::strcmp(timeline->name(), timeline_name) == 0) {
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
        auto* timeline = player->timeline();
        if (timeline && std::strcmp(timeline->name(), timeline_name) == 0 &&
            player->is_playing()) {
            return true;
        }
    }
    return false;
}

std::vector<TimelinePlayer*> AnimationController::get_players_for_target(
    Node* target) const {
    std::vector<TimelinePlayer*> result;
    if (!target) {
        return result;
    }

    result.reserve(players_.size());
    for (auto& player : players_) {
        if (!player->is_finished() && player->target() == target) {
            result.push_back(player.get());
        }
    }
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
