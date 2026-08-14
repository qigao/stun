#include "flex/animation/playback.h"

#include <algorithm>
#include <cmath>

namespace flex::animation {
namespace {

void visit(PlaybackIntervalVisitor visitor, void* user, float from, float to,
           bool forward) {
    if (visitor && from != to) {
        visitor(user, PlaybackInterval{from, to, forward});
    }
}

} // namespace

void PlaybackCursor::play() {
    playing_ = true;
    finished_ = false;
    previous_time_ = -1.0f;
}

void PlaybackCursor::pause() {
    playing_ = false;
}

void PlaybackCursor::stop() {
    playing_ = false;
    finished_ = true;
    time_ = 0.0f;
    previous_time_ = 0.0f;
    reverse_ = false;
}

void PlaybackCursor::seek(float time, float duration) {
    const float upper = std::isfinite(duration) && duration > 0.0f ? duration : 0.0f;
    time_ = std::clamp(std::isfinite(time) ? time : 0.0f, 0.0f, upper);
    previous_time_ = time_;
}

AdvanceResult PlaybackCursor::advance(float delta_time, float duration, float rate,
                                      LoopMode loop_mode,
                                      PlaybackIntervalVisitor visitor,
                                      void* visitor_user,
                                      std::size_t max_intervals) {
    if (!playing_ || finished_) {
        return {false, true};
    }

    previous_time_ = time_;
    if (!std::isfinite(delta_time) || !std::isfinite(duration) ||
        !std::isfinite(rate) || delta_time < 0.0f || duration <= 0.0f ||
        rate < 0.0f || max_intervals == 0) {
        playing_ = false;
        finished_ = true;
        time_ = 0.0f;
        previous_time_ = time_;
        reverse_ = false;
        return {false, false};
    }

    float remaining = delta_time * rate;
    if (remaining <= 0.0f) {
        return {true, true};
    }

    if (loop_mode == LoopMode::Once) {
        const float next = std::min(time_ + remaining, duration);
        visit(visitor, visitor_user, time_, next, true);
        time_ = next;
        if (time_ >= duration) {
            time_ = duration;
            playing_ = false;
            finished_ = true;
        }
        return {playing_ && !finished_, true};
    }

    std::size_t interval_count = 0;
    while (remaining > 0.0f) {
        if (interval_count++ >= max_intervals) {
            playing_ = false;
            finished_ = true;
            previous_time_ = time_;
            return {false, false};
        }

        if (loop_mode == LoopMode::Loop) {
            if (time_ >= duration) {
                time_ = 0.0f;
            }
            const float distance = duration - time_;
            const float step = std::min(remaining, distance);
            visit(visitor, visitor_user, time_, time_ + step, true);
            time_ += step;
            remaining -= step;
            if (time_ >= duration) {
                time_ = 0.0f;
            }
            continue;
        }

        const float distance = reverse_ ? time_ : duration - time_;
        const float step = std::min(remaining, distance);
        const float next = reverse_ ? time_ - step : time_ + step;
        visit(visitor, visitor_user, time_, next, !reverse_);
        time_ = next;
        remaining -= step;

        if ((!reverse_ && time_ >= duration) || (reverse_ && time_ <= 0.0f)) {
            time_ = reverse_ ? 0.0f : duration;
            reverse_ = !reverse_;
        }
    }

    return {true, true};
}

} // namespace flex::animation
