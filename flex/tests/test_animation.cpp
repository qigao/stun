#include "flex/animation.h"
#include "tinytest.h"

#include <stdexcept>
#include <vector>

using namespace flex::animation;

namespace {

void collect_interval(void* user, const PlaybackInterval& interval) {
    static_cast<std::vector<PlaybackInterval>*>(user)->push_back(interval);
}

} // namespace

suite("flex::animation") {
    it("interpolates scalar values for UI adapters") {
        check_float_eq(interpolate(10.0f, 20.0f, 0.25f), 12.5f, 0.001f);
    }

    it("samples a MIR animation expression from fixed numeric inputs") {
        NumericExpression expression(
            "lerp(from, to, clamp(progress, 0, 1)) + sin(time)");

        check(expression.uses_jit());
        check_float_eq(expression.sample({0.0f, 0.25f, 10.0f, 30.0f}),
                       15.0f, 0.001f);
        check_float_eq(expression.sample({0.0f, 2.0f, 10.0f, 30.0f}),
                       30.0f, 0.001f);
    }

    it("rejects invalid and non-finite animation expressions") {
        check_throws_as(NumericExpression("progress + ("), std::invalid_argument);

        NumericExpression expression("sqrt(-1) + progress");
        check_throws_as(expression.sample({}), std::runtime_error);
    }

    it("rejects a non-positive duration") {
        PlaybackCursor cursor;
        cursor.play();

        const auto result = cursor.advance(0.5f, 0.0f, 1.0f, LoopMode::Loop);

        check_false(result.valid);
        check_false(result.active);
        check(cursor.finished());
    }

    it("rejects traversal beyond the configured interval budget") {
        PlaybackCursor cursor;
        cursor.play();

        const auto result = cursor.advance(3.0f, 1.0f, 1.0f, LoopMode::Loop,
                                           nullptr, nullptr, 2);

        check_false(result.valid);
        check_false(result.active);
        check(cursor.finished());
    }

    it("reports every loop interval crossed by a large step") {
        PlaybackCursor cursor;
        std::vector<PlaybackInterval> intervals;
        cursor.play();

        const auto result = cursor.advance(2.5f, 1.0f, 1.0f, LoopMode::Loop,
                                           collect_interval, &intervals);

        check(result.valid);
        check(result.active);
        check_float_eq(cursor.time(), 0.5f, 0.001f);
        check(intervals.size() == 3);
        check_float_eq(intervals[0].from, 0.0f, 0.001f);
        check_float_eq(intervals[0].to, 1.0f, 0.001f);
        check_float_eq(intervals[2].to, 0.5f, 0.001f);
    }

    it("reflects across multiple ping-pong boundaries") {
        PlaybackCursor cursor;
        std::vector<PlaybackInterval> intervals;
        cursor.play();

        const auto result = cursor.advance(3.25f, 1.0f, 1.0f, LoopMode::PingPong,
                                           collect_interval, &intervals);

        check(result.valid);
        check(result.active);
        check_float_eq(cursor.time(), 0.75f, 0.001f);
        check(cursor.reverse());
        check(intervals.size() == 4);
        check(intervals[0].forward);
        check_false(intervals[1].forward);
        check(intervals[2].forward);
        check_false(intervals[3].forward);
    }
}
