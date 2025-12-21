/*
 * Path Animation System Implementation
 */

#include <flex/path.h>
#include <algorithm>
#include <cmath>

namespace flex {

void Path::add_point(float x, float y, float time) {
    PathPoint p;
    p.x = x;
    p.y = y;

    // Auto-calculate time if not specified
    if (time < 0) {
        if (points_.empty()) {
            p.time = 0.0f;
        } else {
            p.time = 1.0f;
        }
    } else {
        p.time = time;
    }

    points_.push_back(p);
}

void Path::add_point(const PathPoint& point) {
    points_.push_back(point);
}

PathPoint Path::interpolate(float t) const {
    if (!is_valid()) {
        return PathPoint(0, 0, 0);
    }

    // Clamp t to [0, 1]
    t = std::max(0.0f, std::min(1.0f, t));

    // Use selected interpolation mode
    switch (mode_) {
        case InterpolationMode::Linear:
            return interpolate_linear(t);
        case InterpolationMode::CatmullRom:
            return interpolate_catmull_rom(t);
        case InterpolationMode::Bezier:
            // For now, fall back to Catmull-Rom
            return interpolate_catmull_rom(t);
        default:
            return interpolate_linear(t);
    }
}

PathPoint Path::interpolate_linear(float t) const {
    // Find the two points to interpolate between
    size_t i = 0;
    for (i = 0; i < points_.size() - 1; i++) {
        if (t <= points_[i + 1].time) {
            break;
        }
    }

    // If we're at or past the last point
    if (i >= points_.size() - 1) {
        return points_.back();
    }

    const PathPoint& p0 = points_[i];
    const PathPoint& p1 = points_[i + 1];

    // Calculate local interpolation factor
    float time_range = p1.time - p0.time;
    float local_t = 0;

    if (time_range > 0.0001f) {
        local_t = (t - p0.time) / time_range;
    }

    // Linear interpolation
    PathPoint result;
    result.x = p0.x + (p1.x - p0.x) * local_t;
    result.y = p0.y + (p1.y - p0.y) * local_t;
    result.time = t;

    return result;
}

PathPoint Path::interpolate_catmull_rom(float t) const {
    if (points_.size() < 2) {
        return PathPoint(0, 0, 0);
    }

    // Find segment index based on time
    size_t i = 0;
    for (i = 0; i < points_.size() - 1; i++) {
        if (t <= points_[i + 1].time) {
            break;
        }
    }

    if (i >= points_.size() - 1) {
        return points_.back();
    }

    // Get 4 control points for Catmull-Rom spline
    // p0, p1, p2, p3 where we interpolate between p1 and p2
    const PathPoint& p1 = points_[i];
    const PathPoint& p2 = points_[i + 1];

    // For endpoints, duplicate the endpoint
    const PathPoint& p0 = (i > 0) ? points_[i - 1] : p1;
    const PathPoint& p3 = (i + 2 < points_.size()) ? points_[i + 2] : p2;

    // Calculate local t within segment
    float time_range = p2.time - p1.time;
    float local_t = 0;

    if (time_range > 0.0001f) {
        local_t = (t - p1.time) / time_range;
    }

    // Catmull-Rom spline formula
    float t2 = local_t * local_t;
    float t3 = t2 * local_t;

    // Calculate x coordinate
    float x = 0.5f * (
        (2.0f * p1.x) +
        (-p0.x + p2.x) * local_t +
        (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
        (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3
    );

    // Calculate y coordinate
    float y = 0.5f * (
        (2.0f * p1.y) +
        (-p0.y + p2.y) * local_t +
        (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
        (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3
    );

    PathPoint result;
    result.x = x;
    result.y = y;
    result.time = t;

    return result;
}

PathAnimation::PathAnimation(Path* path, float duration)
    : path_(path)
    , duration_(duration)
    , elapsed_(0)
    , playing_(false)
    , loop_(false)
{
}

PathPoint PathAnimation::update(float dt) {
    if (playing_ && path_ && path_->is_valid()) {
        elapsed_ += dt;

        if (elapsed_ >= duration_) {
            if (loop_) {
                elapsed_ = std::fmod(elapsed_, duration_);
            } else {
                elapsed_ = duration_;
                playing_ = false;
            }
        }
    }

    float t = progress();

    if (path_ && path_->is_valid()) {
        return path_->interpolate(t);
    }

    return PathPoint(0, 0, 0);
}

} // namespace flex
