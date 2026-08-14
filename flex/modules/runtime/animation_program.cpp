#include "flex/core/animation_program.h"

#include "animation_sampling.h"
#include "flex/animation/numeric_expression.h"
#include "flex/core/cubic_bezier.h"

#include <stdexcept>
#include <utility>

namespace flex {
namespace {

void validate_segment(size_t offset, size_t count, size_t available,
                      const char* message) {
    if (offset > available || count > available - offset) {
        throw std::logic_error(message);
    }
}

// Upper-bound lookup: O(log keyframe_count) time and O(1) space.
template <typename TypedKeyframe>
void find_keyframes(const TypedKeyframe* keyframes, size_t keyframe_count,
                    float time, size_t* previous, size_t* next) {
    size_t left = 0;
    size_t right = keyframe_count;
    while (left < right) {
        const size_t middle = left + (right - left) / 2;
        if (keyframes[middle].time <= time) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }

    if (left == 0) {
        *previous = *next = 0;
    } else if (left >= keyframe_count) {
        *previous = *next = keyframe_count - 1;
    } else {
        *previous = left - 1;
        *next = left;
    }
}

template <typename TypedKeyframe>
float interpolation_progress(const TypedKeyframe* keyframes, size_t previous,
                             size_t next, float time) {
    const float duration = keyframes[next].time - keyframes[previous].time;
    const float linear = duration > 0.0f
                             ? (time - keyframes[previous].time) / duration
                             : 0.0f;
    return keyframes[previous].easing.evaluate(linear);
}

} // namespace

AnimationProgram::AnimationProgram(
    uint64_t source_revision, std::vector<TrackOperation> operations,
    Storage storage)
    : source_revision_(source_revision), operations_(std::move(operations)),
      keyframe_count_(storage.keyframe_count),
      scalar_keyframes_(std::move(storage.scalar_keyframes)),
      vec2_keyframes_(std::move(storage.vec2_keyframes)),
      color_keyframes_(std::move(storage.color_keyframes)),
      vec2_tangents_(std::move(storage.vec2_tangents)),
      vec2_tangent_flags_(std::move(storage.vec2_tangent_flags)),
      generic_keyframes_(std::move(storage.generic_keyframes)) {
    if (vec2_keyframes_.size() != vec2_tangents_.size() ||
        vec2_keyframes_.size() != vec2_tangent_flags_.size()) {
        throw std::logic_error(
            "compiled animation Vec2 tangent storage sizes differ");
    }

    size_t logical_offset = 0;
    for (const auto& operation : operations_) {
        if (operation.keyframe_offset != logical_offset ||
            logical_offset > keyframe_count_ ||
            operation.keyframe_count > keyframe_count_ - logical_offset) {
            throw std::logic_error(
                "compiled animation logical keyframe segment is invalid");
        }
        logical_offset += operation.keyframe_count;

        switch (operation.value_kind) {
        case ValueKind::Empty:
            if (operation.keyframe_count != 0) {
                throw std::logic_error(
                    "compiled empty animation operation has keyframes");
            }
            break;
        case ValueKind::Scalar:
            validate_segment(operation.storage_value_offset,
                             operation.keyframe_count,
                             scalar_keyframes_.size(),
                             "compiled scalar keyframe segment is out of range");
            break;
        case ValueKind::Vec2:
            validate_segment(operation.storage_value_offset,
                             operation.keyframe_count, vec2_keyframes_.size(),
                             "compiled Vec2 keyframe segment is out of range");
            break;
        case ValueKind::Color:
            validate_segment(operation.storage_value_offset,
                             operation.keyframe_count, color_keyframes_.size(),
                             "compiled color keyframe segment is out of range");
            break;
        case ValueKind::Generic:
            validate_segment(operation.storage_value_offset,
                             operation.keyframe_count,
                             generic_keyframes_.size(),
                             "compiled generic keyframe segment is out of range");
            break;
        }
    }
    if (logical_offset != keyframe_count_) {
        throw std::logic_error(
            "compiled animation logical keyframe count is inconsistent");
    }
}

AnimValue AnimationProgram::sample(size_t operation_index, float time) const {
    if (operation_index >= operations_.size()) {
        throw std::out_of_range(
            "compiled animation operation index is out of range");
    }
    return sample_unchecked(operation_index, time);
}

void AnimationProgram::sample_all(float time, AnimValue* outputs,
                                  size_t output_count) const {
    if (output_count != operations_.size()) {
        throw std::invalid_argument(
            "compiled animation output count must equal track count");
    }
    if (output_count != 0 && !outputs) {
        throw std::invalid_argument(
            "compiled animation output storage is null");
    }
    for (size_t index = 0; index < operations_.size(); ++index) {
        outputs[index] = sample_unchecked(index, time);
    }
}

AnimValue AnimationProgram::sample_unchecked(size_t operation_index,
                                             float time) const {
    const auto& operation = operations_[operation_index];
    if (operation.value_kind == ValueKind::Empty) {
        return AnimValue(0.0f);
    }
    if (operation.value_kind == ValueKind::Generic) {
        const Keyframe* segment = generic_keyframes_.data() +
                                  operation.storage_value_offset;
        return sample_keyframes(segment, operation.keyframe_count,
                                operation.numeric_expression.get(),
                                operation.spatial_interpolation, time);
    }

    if (operation.value_kind == ValueKind::Scalar) {
        const ScalarKeyframe* keyframes =
            scalar_keyframes_.data() + operation.storage_value_offset;
        size_t previous = 0;
        size_t next = 0;
        find_keyframes(keyframes, operation.keyframe_count, time, &previous,
                       &next);
        if (previous == next) {
            return AnimValue(keyframes[previous].value);
        }
        const float progress =
            interpolation_progress(keyframes, previous, next, time);
        if (operation.numeric_expression) {
            return AnimValue(operation.numeric_expression->sample(
                {time, progress, keyframes[previous].value,
                 keyframes[next].value}));
        }
        return AnimValue(keyframes[previous].value +
                         (keyframes[next].value - keyframes[previous].value) *
                             progress);
    }

    if (operation.value_kind == ValueKind::Vec2) {
        const Vec2Keyframe* keyframes =
            vec2_keyframes_.data() + operation.storage_value_offset;
        size_t previous = 0;
        size_t next = 0;
        find_keyframes(keyframes, operation.keyframe_count, time, &previous,
                       &next);
        if (previous == next) {
            return AnimValue(keyframes[previous].value);
        }
        const float progress =
            interpolation_progress(keyframes, previous, next, time);
        if (operation.spatial_interpolation ==
            SpatialInterpolation::CatmullRom) {
            const Vec2& p0 = previous > 0 ? keyframes[previous - 1].value
                                          : keyframes[previous].value;
            const Vec2& p3 = next + 1 < operation.keyframe_count
                                 ? keyframes[next + 1].value
                                 : keyframes[next].value;
            return AnimValue(animation_sampling::catmull_rom(
                p0, keyframes[previous].value, keyframes[next].value, p3,
                progress));
        }
        if (operation.spatial_interpolation ==
            SpatialInterpolation::CubicBezier) {
            const size_t tangent_offset = operation.storage_value_offset;
            const auto* tangents = vec2_tangents_.data() + tangent_offset;
            const auto* tangent_flags =
                vec2_tangent_flags_.data() + tangent_offset;
            if (!tangent_flags[previous] || !tangent_flags[next]) {
                throw std::logic_error(
                    "cubicBezier tracks require explicit tangents on every keyframe");
            }
            const Vec2 control1 =
                keyframes[previous].value + tangents[previous].out;
            const Vec2 control2 = keyframes[next].value + tangents[next].in;
            return AnimValue(CubicBezier2D(keyframes[previous].value, control1,
                                           control2, keyframes[next].value)
                                 .position(progress));
        }
        return AnimValue(animation_sampling::lerp_vec2(
            keyframes[previous].value, keyframes[next].value, progress));
    }

    if (operation.value_kind == ValueKind::Color) {
        const ColorKeyframe* keyframes =
            color_keyframes_.data() + operation.storage_value_offset;
        size_t previous = 0;
        size_t next = 0;
        find_keyframes(keyframes, operation.keyframe_count, time, &previous,
                       &next);
        if (previous == next) {
            return AnimValue(keyframes[previous].value);
        }
        const float progress =
            interpolation_progress(keyframes, previous, next, time);
        return AnimValue(animation_sampling::lerp_color(
            keyframes[previous].value, keyframes[next].value, progress));
    }

    throw std::logic_error("compiled animation operation has an invalid kind");
}

} // namespace flex
