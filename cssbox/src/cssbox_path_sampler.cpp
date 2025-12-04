#include "cssbox_path_sampler.h"
#include "cssbox_svg_path.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace cssbox {

PathSampler::PathSampler() 
    : total_length_(0)
    , step_size_(1.0f)
    , current_x_(0)
    , current_y_(0)
    , current_distance_(0)
{
}

std::vector<PathSample> PathSampler::sample_path(const std::string& path_d, float step) {
    samples_.clear();
    total_length_ = 0;
    step_size_ = (step > 0.0f) ? step : 1.0f;  // Prevent divide-by-zero
    current_x_ = 0;
    current_y_ = 0;
    current_distance_ = 0;
    
    // Parse the path
    auto commands = SVGPathParser::parse(path_d);
    
    float start_x = 0, start_y = 0;
    float last_control_x = 0, last_control_y = 0;
    
    for (const auto& cmd : commands) {
        float x, y, x1, y1, x2, y2;
        
        switch (cmd.type) {
            case 'M':  // MoveTo
                x = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                y = cmd.relative ? current_y_ + cmd.params[1] : cmd.params[1];
                current_x_ = start_x = x;
                current_y_ = start_y = y;
                last_control_x = current_x_;
                last_control_y = current_y_;
                // Add initial point
                add_sample(current_x_, current_y_, 0);
                break;
                
            case 'L':  // LineTo
                x = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                y = cmd.relative ? current_y_ + cmd.params[1] : cmd.params[1];
                sample_line(current_x_, current_y_, x, y);
                current_x_ = x;
                current_y_ = y;
                last_control_x = current_x_;
                last_control_y = current_y_;
                break;
                
            case 'H':  // Horizontal line
                x = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                sample_line(current_x_, current_y_, x, current_y_);
                current_x_ = x;
                last_control_x = current_x_;
                last_control_y = current_y_;
                break;
                
            case 'V':  // Vertical line
                y = cmd.relative ? current_y_ + cmd.params[0] : cmd.params[0];
                sample_line(current_x_, current_y_, current_x_, y);
                current_y_ = y;
                last_control_x = current_x_;
                last_control_y = current_y_;
                break;
                
            case 'C':  // Cubic Bezier
                x1 = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                y1 = cmd.relative ? current_y_ + cmd.params[1] : cmd.params[1];
                x2 = cmd.relative ? current_x_ + cmd.params[2] : cmd.params[2];
                y2 = cmd.relative ? current_y_ + cmd.params[3] : cmd.params[3];
                x = cmd.relative ? current_x_ + cmd.params[4] : cmd.params[4];
                y = cmd.relative ? current_y_ + cmd.params[5] : cmd.params[5];
                sample_cubic_bezier(current_x_, current_y_, x1, y1, x2, y2, x, y);
                last_control_x = x2;
                last_control_y = y2;
                current_x_ = x;
                current_y_ = y;
                break;
                
            case 'S':  // Smooth Cubic Bezier
                x1 = 2 * current_x_ - last_control_x;
                y1 = 2 * current_y_ - last_control_y;
                x2 = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                y2 = cmd.relative ? current_y_ + cmd.params[1] : cmd.params[1];
                x = cmd.relative ? current_x_ + cmd.params[2] : cmd.params[2];
                y = cmd.relative ? current_y_ + cmd.params[3] : cmd.params[3];
                sample_cubic_bezier(current_x_, current_y_, x1, y1, x2, y2, x, y);
                last_control_x = x2;
                last_control_y = y2;
                current_x_ = x;
                current_y_ = y;
                break;
                
            case 'Q':  // Quadratic Bezier
                x1 = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                y1 = cmd.relative ? current_y_ + cmd.params[1] : cmd.params[1];
                x = cmd.relative ? current_x_ + cmd.params[2] : cmd.params[2];
                y = cmd.relative ? current_y_ + cmd.params[3] : cmd.params[3];
                sample_quadratic_bezier(current_x_, current_y_, x1, y1, x, y);
                last_control_x = x1;
                last_control_y = y1;
                current_x_ = x;
                current_y_ = y;
                break;
                
            case 'T':  // Smooth Quadratic Bezier
                x1 = 2 * current_x_ - last_control_x;
                y1 = 2 * current_y_ - last_control_y;
                x = cmd.relative ? current_x_ + cmd.params[0] : cmd.params[0];
                y = cmd.relative ? current_y_ + cmd.params[1] : cmd.params[1];
                sample_quadratic_bezier(current_x_, current_y_, x1, y1, x, y);
                last_control_x = x1;
                last_control_y = y1;
                current_x_ = x;
                current_y_ = y;
                break;
                
            case 'Z':  // ClosePath
                sample_line(current_x_, current_y_, start_x, start_y);
                current_x_ = start_x;
                current_y_ = start_y;
                last_control_x = current_x_;
                last_control_y = current_y_;
                break;
                
            // Note: Arc command not yet implemented for sampling
            // Would require arc-to-bezier conversion or direct arc sampling
        }
    }
    
    total_length_ = current_distance_;
    return samples_;
}

void PathSampler::sample_line(float x1, float y1, float x2, float y2) {
    float len = distance(x1, y1, x2, y2);
    if (len < 0.001f) return;  // Skip zero-length lines
    
    float angle = angle_between(x1, y1, x2, y2);
    int num_steps = std::max(1, (int)(len / step_size_));
    
    for (int i = 1; i <= num_steps; i++) {
        float t = (float)i / num_steps;
        float x = x1 + (x2 - x1) * t;
        float y = y1 + (y2 - y1) * t;
        add_sample(x, y, angle);
    }
}

void PathSampler::sample_cubic_bezier(float x1, float y1, float cx1, float cy1,
                                      float cx2, float cy2, float x2, float y2) {
    // Adaptive sampling based on curve complexity
    // Estimate curve length (rough approximation)
    float chord_len = distance(x1, y1, x2, y2);
    float control_len = distance(x1, y1, cx1, cy1) + 
                       distance(cx1, cy1, cx2, cy2) + 
                       distance(cx2, cy2, x2, y2);
    float approx_len = (chord_len + control_len) / 2.0f;
    
    int num_steps = std::max(2, (int)(approx_len / step_size_));
    
    for (int i = 1; i <= num_steps; i++) {
        float t = (float)i / num_steps;
        float t1 = 1.0f - t;
        
        // De Casteljau's algorithm for cubic bezier
        float x = t1*t1*t1*x1 + 3*t1*t1*t*cx1 + 3*t1*t*t*cx2 + t*t*t*x2;
        float y = t1*t1*t1*y1 + 3*t1*t1*t*cy1 + 3*t1*t*t*cy2 + t*t*t*y2;
        
        // Calculate tangent (derivative)
        float dx = 3*t1*t1*(cx1-x1) + 6*t1*t*(cx2-cx1) + 3*t*t*(x2-cx2);
        float dy = 3*t1*t1*(cy1-y1) + 6*t1*t*(cy2-cy1) + 3*t*t*(y2-cy2);
        float angle = atan2f(dy, dx);
        
        add_sample(x, y, angle);
    }
}

void PathSampler::sample_quadratic_bezier(float x1, float y1, float cx, float cy,
                                          float x2, float y2) {
    // Estimate curve length
    float chord_len = distance(x1, y1, x2, y2);
    float control_len = distance(x1, y1, cx, cy) + distance(cx, cy, x2, y2);
    float approx_len = (chord_len + control_len) / 2.0f;
    
    int num_steps = std::max(2, (int)(approx_len / step_size_));
    
    for (int i = 1; i <= num_steps; i++) {
        float t = (float)i / num_steps;
        float t1 = 1.0f - t;
        
        // Quadratic bezier formula
        float x = t1*t1*x1 + 2*t1*t*cx + t*t*x2;
        float y = t1*t1*y1 + 2*t1*t*cy + t*t*y2;
        
        // Calculate tangent (derivative)
        float dx = 2*t1*(cx-x1) + 2*t*(x2-cx);
        float dy = 2*t1*(cy-y1) + 2*t*(y2-cy);
        float angle = atan2f(dy, dx);
        
        add_sample(x, y, angle);
    }
}

void PathSampler::add_sample(float x, float y, float angle) {
    // Calculate distance from last sample
    if (!samples_.empty()) {
        const auto& last = samples_.back();
        float dist = distance(last.x, last.y, x, y);
        current_distance_ += dist;
    }
    
    samples_.emplace_back(x, y, angle, current_distance_);
}

float PathSampler::distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

float PathSampler::angle_between(float x1, float y1, float x2, float y2) {
    return atan2f(y2 - y1, x2 - x1);
}

// Interpolation function
PathSample interpolate_path_position(const std::vector<PathSample>& samples, float distance) {
    if (samples.empty()) {
        return PathSample();
    }
    
    if (distance <= 0) {
        return samples.front();
    }
    
    if (distance >= samples.back().distance) {
        return samples.back();
    }
    
    // Binary search for the segment containing the distance
    size_t left = 0;
    size_t right = samples.size() - 1;
    
    while (left < right - 1) {
        size_t mid = (left + right) / 2;
        if (samples[mid].distance < distance) {
            left = mid;
        } else {
            right = mid;
        }
    }
    
    // Linear interpolation between samples[left] and samples[right]
    const auto& s1 = samples[left];
    const auto& s2 = samples[right];
    
    float segment_len = s2.distance - s1.distance;
    if (segment_len < 0.001f) {
        return s1;  // Avoid division by zero
    }
    
    float t = (distance - s1.distance) / segment_len;

    PathSample result;
    result.x = s1.x + (s2.x - s1.x) * t;
    result.y = s1.y + (s2.y - s1.y) * t;

    // Angle interpolation with wrap-around handling (avoid 360° -> 0° flip)
    float angle_diff = s2.angle - s1.angle;
    if (angle_diff > M_PI) angle_diff -= 2 * M_PI;
    if (angle_diff < -M_PI) angle_diff += 2 * M_PI;
    result.angle = s1.angle + angle_diff * t;

    result.distance = distance;
    
    return result;
}

} // namespace cssbox
