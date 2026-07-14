/*
 * Flex Animation - Backend-neutral playback clock
 *
 * This layer owns playback time and loop traversal only. Property lookup and
 * writes belong to adapters in flex runtime or flexUI.
 */

#pragma once

#include <cstddef>

namespace flex::animation {

/** Linearly interpolate a scalar value. Progress is intentionally not clamped. */
constexpr float interpolate(float from, float to, float progress) {
    return from + (to - from) * progress;
}

constexpr std::size_t default_max_playback_intervals = 4096;

enum class LoopMode {
    Once,
    Loop,
    PingPong,
};

struct PlaybackInterval {
    float from = 0.0f;
    float to = 0.0f;
    bool forward = true;
};

using PlaybackIntervalVisitor = void (*)(void* user, const PlaybackInterval& interval);

struct AdvanceResult {
    bool active = false;
    bool valid = true;
};

class PlaybackCursor {
public:
    float time() const { return time_; }
    float previous_time() const { return previous_time_; }
    bool playing() const { return playing_; }
    bool finished() const { return finished_; }
    bool reverse() const { return reverse_; }

    /** Start or resume playback at the current position. */
    void play();
    void pause();
    /** Stop playback and reset the cursor to the beginning. */
    void stop();
    /** Seek to a clamped position without emitting traversal intervals. */
    void seek(float time, float duration);

    /**
     * Advance the cursor and report each traversed monotonic interval.
     *
     * Returns valid=false and stops playback for non-finite/negative inputs,
     * non-positive duration, negative rate, or an interval limit violation.
     * The visitor is borrowed and invoked synchronously.
     */
    AdvanceResult advance(float delta_time, float duration, float rate,
                          LoopMode loop_mode,
                          PlaybackIntervalVisitor visitor = nullptr,
                          void* visitor_user = nullptr,
                          std::size_t max_intervals = default_max_playback_intervals);

private:
    float time_ = 0.0f;
    float previous_time_ = 0.0f;
    bool playing_ = false;
    bool finished_ = false;
    bool reverse_ = false;
};

} // namespace flex::animation
