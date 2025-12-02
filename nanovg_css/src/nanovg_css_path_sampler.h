#ifndef NANOVG_CSS_PATH_SAMPLER_H
#define NANOVG_CSS_PATH_SAMPLER_H

#include <vector>
#include <string>

namespace nvgcss {

/**
 * @brief A sample point along a path with position and tangent information
 */
struct PathSample {
    float x, y;           // Position
    float angle;          // Tangent angle in radians
    float distance;       // Cumulative distance from path start
    
    PathSample() : x(0), y(0), angle(0), distance(0) {}
    PathSample(float px, float py, float a, float d) 
        : x(px), y(py), angle(a), distance(d) {}
};

/**
 * @brief Samples an SVG path into a sequence of points with tangent information
 * 
 * This is used for text-on-path rendering and other path-following effects.
 */
class PathSampler {
public:
    PathSampler();
    
    /**
     * @brief Sample an SVG path string into points
     * @param path_d SVG path data (d attribute)
     * @param step Sampling step size in pixels (smaller = more accurate)
     * @return Vector of path samples
     */
    std::vector<PathSample> sample_path(const std::string& path_d, float step = 1.0f);
    
    /**
     * @brief Get total path length
     */
    float get_total_length() const { return total_length_; }
    
private:
    std::vector<PathSample> samples_;
    float total_length_;
    float step_size_;
    
    // Current position tracking
    float current_x_, current_y_;
    float current_distance_;
    
    // Sampling methods for different path commands
    void sample_line(float x1, float y1, float x2, float y2);
    void sample_cubic_bezier(float x1, float y1, float cx1, float cy1,
                             float cx2, float cy2, float x2, float y2);
    void sample_quadratic_bezier(float x1, float y1, float cx, float cy,
                                 float x2, float y2);
    
    // Helper: Add a sample point
    void add_sample(float x, float y, float angle);
    
    // Helper: Calculate distance between two points
    static float distance(float x1, float y1, float x2, float y2);
    
    // Helper: Calculate angle from point 1 to point 2
    static float angle_between(float x1, float y1, float x2, float y2);
};

/**
 * @brief Interpolate position along a sampled path
 * @param samples Path samples
 * @param distance Distance along path
 * @return Interpolated sample at the given distance
 */
PathSample interpolate_path_position(const std::vector<PathSample>& samples, float distance);

} // namespace nvgcss

#endif // NANOVG_CSS_PATH_SAMPLER_H
