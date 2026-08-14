/*
 * Flex Engine - Compiled Animation Program
 *
 * Immutable, owning track operation and typed keyframe data shared by
 * Timeline execution paths.
 */
#pragma once

#include "flex/animation/keyframe.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace flex {

class Timeline;
class TimelinePlayer;
namespace animation {
class NumericExpression;
}

class AnimationProgram final {
public:
    enum class ValueKind : uint8_t {
        Empty,
        Scalar,
        Vec2,
        Color,
        Generic,
    };

    struct TrackOperation {
        // Logical offset in the source snapshot. Typed storage offsets are
        // internal so callers do not depend on the physical layout.
        size_t keyframe_offset = 0;
        size_t keyframe_count = 0;
        ValueKind value_kind = ValueKind::Empty;
        size_t storage_value_offset = 0;
        PropertyID property_id = PropertyID::Unknown;
        SpatialInterpolation spatial_interpolation =
            SpatialInterpolation::Linear;
        bool has_target_selector = false;
        std::shared_ptr<animation::NumericExpression> numeric_expression;
        std::string target_id;
    };

    AnimationProgram(const AnimationProgram&) = delete;
    AnimationProgram& operator=(const AnimationProgram&) = delete;
    AnimationProgram(AnimationProgram&&) = delete;
    AnimationProgram& operator=(AnimationProgram&&) = delete;

    uint64_t source_revision() const { return source_revision_; }
    size_t track_count() const { return operations_.size(); }
    size_t keyframe_count() const { return keyframe_count_; }
    size_t scalar_keyframe_count() const { return scalar_keyframes_.size(); }
    size_t vec2_keyframe_count() const { return vec2_keyframes_.size(); }
    size_t color_keyframe_count() const { return color_keyframes_.size(); }
    size_t generic_keyframe_count() const { return generic_keyframes_.size(); }
    const std::vector<TrackOperation>& operations() const { return operations_; }

    // O(log keyframes) time and O(1) auxiliary space. Throws std::out_of_range
    // when operation_index does not identify a compiled track.
    AnimValue sample(size_t operation_index, float time) const;

    /**
     * Sample every operation at one time into caller-owned storage.
     *
     * The output count must equal track_count(); a non-empty output must not
     * be null. Argument errors are reported before any output is changed.
     * Sampling errors propagate after earlier outputs may have been written.
     * Complexity is O(sum(log(keyframes_per_track))) time and O(1) auxiliary
     * space. The Program allocates no scratch storage; assigning a generic
     * string result may still allocate according to std::string capacity.
     */
    void sample_all(float time, AnimValue* outputs, size_t output_count) const;

private:
    friend class Timeline;
    friend class TimelinePlayer;

    struct ScalarKeyframe {
        float time;
        float value;
        Easing easing;
    };

    struct Vec2Keyframe {
        float time;
        Vec2 value;
        Easing easing;
    };

    struct ColorKeyframe {
        float time;
        Color value;
        Easing easing;
    };

    struct Storage {
        size_t keyframe_count = 0;
        std::vector<ScalarKeyframe> scalar_keyframes;
        std::vector<Vec2Keyframe> vec2_keyframes;
        std::vector<ColorKeyframe> color_keyframes;
        std::vector<Keyframe::SpatialTangents> vec2_tangents;
        std::vector<uint8_t> vec2_tangent_flags;
        std::vector<Keyframe> generic_keyframes;
    };

    AnimationProgram(uint64_t source_revision,
                     std::vector<TrackOperation> operations, Storage storage);
    AnimValue sample_unchecked(size_t operation_index, float time) const;

    const uint64_t source_revision_;
    const std::vector<TrackOperation> operations_;
    const size_t keyframe_count_;
    const std::vector<ScalarKeyframe> scalar_keyframes_;
    const std::vector<Vec2Keyframe> vec2_keyframes_;
    const std::vector<ColorKeyframe> color_keyframes_;
    const std::vector<Keyframe::SpatialTangents> vec2_tangents_;
    const std::vector<uint8_t> vec2_tangent_flags_;
    const std::vector<Keyframe> generic_keyframes_;
};

} // namespace flex
