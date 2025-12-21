/*
 * Path Animation System
 * Allows nodes to follow predefined paths with linear interpolation
 */

#pragma once

#include <vector>
#include <memory>

namespace flex {

// A point along a path
struct PathPoint {
    float x;
    float y;
    float time;  // Normalized time (0.0 - 1.0) when object reaches this point

    PathPoint() : x(0), y(0), time(0) {}
    PathPoint(float x_, float y_, float t = 0) : x(x_), y(y_), time(t) {}
};

// A path defined by a series of points
class Path {
public:
    // Interpolation mode
    enum class InterpolationMode {
        Linear,         // Linear interpolation between points
        CatmullRom,     // Smooth Catmull-Rom spline
        Bezier          // Cubic Bezier curves
    };

    Path() : mode_(InterpolationMode::Linear) {}

    // Add a point to the path
    void add_point(float x, float y, float time = -1.0f);
    void add_point(const PathPoint& point);

    // Get position along path at normalized time t (0.0 - 1.0)
    // Returns interpolated (x, y) position
    PathPoint interpolate(float t) const;

    // Set interpolation mode
    void set_interpolation_mode(InterpolationMode mode) { mode_ = mode; }
    InterpolationMode interpolation_mode() const { return mode_; }

    // Get number of points
    size_t point_count() const { return points_.size(); }

    // Get specific point
    const PathPoint& point(size_t index) const { return points_[index]; }

    // Clear all points
    void clear() { points_.clear(); }

    // Check if path is valid (has at least 2 points)
    bool is_valid() const { return points_.size() >= 2; }

private:
    std::vector<PathPoint> points_;
    InterpolationMode mode_;

    // Interpolation helpers
    PathPoint interpolate_linear(float t) const;
    PathPoint interpolate_catmull_rom(float t) const;
};

// Path animation controller
class PathAnimation {
public:
    PathAnimation(Path* path, float duration);

    // Update animation (returns current position)
    PathPoint update(float dt);

    // Control
    void play() { playing_ = true; }
    void pause() { playing_ = false; }
    void stop() { elapsed_ = 0; playing_ = false; }
    void reset() { elapsed_ = 0; }

    // State
    bool is_playing() const { return playing_; }
    bool is_finished() const { return elapsed_ >= duration_; }
    float progress() const { return duration_ > 0 ? elapsed_ / duration_ : 0; }

    // Settings
    void set_loop(bool loop) { loop_ = loop; }
    void set_duration(float duration) { duration_ = duration; }

private:
    Path* path_;
    float duration_;    // Total animation duration in seconds
    float elapsed_;     // Elapsed time
    bool playing_;
    bool loop_;
};

} // namespace flex
