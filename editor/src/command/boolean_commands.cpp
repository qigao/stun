/*
 * Boolean Operations Implementation
 *
 * Uses Clipper2 library for robust polygon boolean operations.
 * Converts SVG paths to polygons by flattening bezier curves.
 */

#include <editor/command/boolean_commands.h>
#include <flex/shape.h>
#include <clipper2/clipper.h>
#include <cmath>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace editor {

// ============================================================================
// Path Flattening - Convert bezier curves to line segments
// ============================================================================

using Point2D = Clipper2Lib::PointD;
using PathD = Clipper2Lib::PathD;
using PathsD = Clipper2Lib::PathsD;

// Flatten cubic bezier using de Casteljau algorithm
static void flatten_cubic(PathD& path,
                          double x0, double y0, double x1, double y1,
                          double x2, double y2, double x3, double y3,
                          double tolerance = 1.0, int depth = 0) {
    // Maximum recursion depth
    if (depth > 10) {
        path.push_back({x3, y3});
        return;
    }

    // Check if curve is flat enough
    double dx = x3 - x0;
    double dy = y3 - y0;
    double d1 = std::abs((x1 - x3) * dy - (y1 - y3) * dx);
    double d2 = std::abs((x2 - x3) * dy - (y2 - y3) * dx);

    if ((d1 + d2) * (d1 + d2) < tolerance * tolerance * (dx * dx + dy * dy)) {
        path.push_back({x3, y3});
        return;
    }

    // Subdivide using de Casteljau
    double x01 = (x0 + x1) / 2, y01 = (y0 + y1) / 2;
    double x12 = (x1 + x2) / 2, y12 = (y1 + y2) / 2;
    double x23 = (x2 + x3) / 2, y23 = (y2 + y3) / 2;
    double x012 = (x01 + x12) / 2, y012 = (y01 + y12) / 2;
    double x123 = (x12 + x23) / 2, y123 = (y12 + y23) / 2;
    double x0123 = (x012 + x123) / 2, y0123 = (y012 + y123) / 2;

    flatten_cubic(path, x0, y0, x01, y01, x012, y012, x0123, y0123, tolerance, depth + 1);
    flatten_cubic(path, x0123, y0123, x123, y123, x23, y23, x3, y3, tolerance, depth + 1);
}

// Flatten quadratic bezier (convert to cubic first)
static void flatten_quadratic(PathD& path,
                              double x0, double y0, double x1, double y1,
                              double x2, double y2, double tolerance = 1.0) {
    // Convert quadratic to cubic
    double cx1 = x0 + 2.0/3.0 * (x1 - x0);
    double cy1 = y0 + 2.0/3.0 * (y1 - y0);
    double cx2 = x2 + 2.0/3.0 * (x1 - x2);
    double cy2 = y2 + 2.0/3.0 * (y1 - y2);

    flatten_cubic(path, x0, y0, cx1, cy1, cx2, cy2, x2, y2, tolerance);
}

// Arc to bezier conversion (ported from renderer_thorvg.cpp)
static void arc_to_center(
    double x1, double y1, double x2, double y2,
    double rx, double ry, double phi,
    bool large_arc, bool sweep,
    double* out_cx, double* out_cy, double* out_theta1, double* out_dtheta) {

    if (rx == 0 || ry == 0) {
        *out_cx = x1; *out_cy = y1;
        *out_theta1 = 0; *out_dtheta = 0;
        return;
    }

    rx = std::abs(rx);
    ry = std::abs(ry);

    double cos_phi = std::cos(phi);
    double sin_phi = std::sin(phi);

    double dx = (x1 - x2) / 2.0;
    double dy = (y1 - y2) / 2.0;
    double x1p = cos_phi * dx + sin_phi * dy;
    double y1p = -sin_phi * dx + cos_phi * dy;

    double x1p2 = x1p * x1p;
    double y1p2 = y1p * y1p;
    double rx2 = rx * rx;
    double ry2 = ry * ry;

    double lambda = x1p2 / rx2 + y1p2 / ry2;
    if (lambda > 1) {
        double sqrt_lambda = std::sqrt(lambda);
        rx *= sqrt_lambda;
        ry *= sqrt_lambda;
        rx2 = rx * rx;
        ry2 = ry * ry;
    }

    double num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
    double denom = rx2 * y1p2 + ry2 * x1p2;

    double sq = 0;
    if (denom > 0 && num > 0) {
        sq = std::sqrt(num / denom);
    }
    if (large_arc == sweep) sq = -sq;

    double cxp = sq * rx * y1p / ry;
    double cyp = -sq * ry * x1p / rx;

    double cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0;
    double cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0;

    auto angle = [](double ux, double uy, double vx, double vy) -> double {
        double dot = ux * vx + uy * vy;
        double len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        double ang = (len > 0) ? std::acos(std::clamp(dot / len, -1.0, 1.0)) : 0;
        if (ux * vy - uy * vx < 0) ang = -ang;
        return ang;
    };

    double theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                          (-x1p - cxp) / rx, (-y1p - cyp) / ry);

    if (!sweep && dtheta > 0) dtheta -= 2 * M_PI;
    if (sweep && dtheta < 0) dtheta += 2 * M_PI;

    *out_cx = cx;
    *out_cy = cy;
    *out_theta1 = theta1;
    *out_dtheta = dtheta;
}

// Flatten arc to line segments
static void flatten_arc(PathD& path,
                        double x1, double y1, double x2, double y2,
                        double rx, double ry, double rotation,
                        bool large_arc, bool sweep, double tolerance = 1.0) {
    if (x1 == x2 && y1 == y2) return;
    if (rx == 0 || ry == 0) {
        path.push_back({x2, y2});
        return;
    }

    double phi = rotation * M_PI / 180.0;
    double cx, cy, theta1, dtheta;
    arc_to_center(x1, y1, x2, y2, rx, ry, phi, large_arc, sweep,
                  &cx, &cy, &theta1, &dtheta);

    // Subdivide arc into small segments
    int segments = std::max(1, static_cast<int>(std::ceil(std::abs(dtheta) * rx / tolerance)));
    segments = std::min(segments, 100);  // Cap at 100 segments

    double cos_phi = std::cos(phi);
    double sin_phi = std::sin(phi);

    for (int i = 1; i <= segments; ++i) {
        double t = theta1 + dtheta * i / segments;
        double cos_t = std::cos(t);
        double sin_t = std::sin(t);
        double x = cx + rx * cos_phi * cos_t - ry * sin_phi * sin_t;
        double y = cy + rx * sin_phi * cos_t + ry * cos_phi * sin_t;
        path.push_back({x, y});
    }
}

// ============================================================================
// SVG Path Parser - extracts polygon points
// ============================================================================

static PathsD parse_svg_to_clipper(const std::string& d, const Transform2D& transform) {
    PathsD result;
    if (d.empty()) return result;

    PathD current_path;
    double cx = 0, cy = 0;
    double sx = 0, sy = 0;
    size_t i = 0;
    char cmd = 0;

    auto skip_ws = [&]() {
        while (i < d.size() && (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == ','))
            ++i;
    };

    auto parse_num = [&]() -> double {
        skip_ws();
        size_t start = i;
        if (i < d.size() && (d[i] == '-' || d[i] == '+')) ++i;
        while (i < d.size() && ((d[i] >= '0' && d[i] <= '9') || d[i] == '.')) ++i;
        if (start == i) return 0;
        return std::stod(d.substr(start, i - start));
    };

    // Apply transform to a point
    auto transform_point = [&](double x, double y) -> Point2D {
        double tx = transform.a * x + transform.c * y + transform.tx;
        double ty = transform.b * x + transform.d * y + transform.ty;
        return {tx, ty};
    };

    while (i < d.size()) {
        skip_ws();
        if (i >= d.size()) break;

        char c = d[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            cmd = c;
            ++i;
        }

        bool relative = (cmd >= 'a' && cmd <= 'z');
        char ucmd = relative ? (cmd - 32) : cmd;

        switch (ucmd) {
            case 'M': {
                // Start new subpath
                if (!current_path.empty()) {
                    result.push_back(current_path);
                    current_path.clear();
                }
                double x = parse_num(), y = parse_num();
                if (relative) { x += cx; y += cy; }
                current_path.push_back(transform_point(x, y));
                cx = sx = x; cy = sy = y;
                cmd = relative ? 'l' : 'L';
                break;
            }
            case 'L': {
                double x = parse_num(), y = parse_num();
                if (relative) { x += cx; y += cy; }
                current_path.push_back(transform_point(x, y));
                cx = x; cy = y;
                break;
            }
            case 'H': {
                double x = parse_num();
                if (relative) x += cx;
                current_path.push_back(transform_point(x, cy));
                cx = x;
                break;
            }
            case 'V': {
                double y = parse_num();
                if (relative) y += cy;
                current_path.push_back(transform_point(cx, y));
                cy = y;
                break;
            }
            case 'C': {
                double x1 = parse_num(), y1 = parse_num();
                double x2 = parse_num(), y2 = parse_num();
                double x = parse_num(), y = parse_num();
                if (relative) {
                    x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy;
                }
                // Flatten cubic bezier
                PathD temp;
                flatten_cubic(temp, cx, cy, x1, y1, x2, y2, x, y);
                for (auto& pt : temp) {
                    current_path.push_back(transform_point(pt.x, pt.y));
                }
                cx = x; cy = y;
                break;
            }
            case 'Q': {
                double x1 = parse_num(), y1 = parse_num();
                double x = parse_num(), y = parse_num();
                if (relative) { x1 += cx; y1 += cy; x += cx; y += cy; }
                PathD temp;
                flatten_quadratic(temp, cx, cy, x1, y1, x, y);
                for (auto& pt : temp) {
                    current_path.push_back(transform_point(pt.x, pt.y));
                }
                cx = x; cy = y;
                break;
            }
            case 'A': {
                double rx = parse_num(), ry = parse_num();
                double rotation = parse_num();
                double large_arc_flag = parse_num();
                double sweep_flag = parse_num();
                double x = parse_num(), y = parse_num();
                if (relative) { x += cx; y += cy; }
                PathD temp;
                flatten_arc(temp, cx, cy, x, y, rx, ry, rotation,
                            large_arc_flag != 0, sweep_flag != 0);
                for (auto& pt : temp) {
                    current_path.push_back(transform_point(pt.x, pt.y));
                }
                cx = x; cy = y;
                break;
            }
            case 'Z': {
                // Close path - no need to add point, Clipper handles closed paths
                if (!current_path.empty()) {
                    result.push_back(current_path);
                    current_path.clear();
                }
                cx = sx; cy = sy;
                break;
            }
            default:
                ++i;
                break;
        }
    }

    if (!current_path.empty()) {
        result.push_back(current_path);
    }

    return result;
}

// ============================================================================
// Clipper to SVG Path conversion
// ============================================================================

static std::string clipper_to_svg_path(const PathsD& paths) {
    std::ostringstream ss;
    ss << std::fixed;
    ss.precision(2);

    for (const auto& path : paths) {
        if (path.empty()) continue;

        ss << "M " << path[0].x << " " << path[0].y;
        for (size_t i = 1; i < path.size(); ++i) {
            ss << " L " << path[i].x << " " << path[i].y;
        }
        ss << " Z ";
    }

    return ss.str();
}

// ============================================================================
// Boolean Operation Implementation
// ============================================================================

ShapeNode::Ptr perform_boolean_operation(
    const std::vector<ShapeNode::Ptr>& shapes,
    BooleanOp op) {

    if (shapes.size() < 2) return nullptr;

    // Convert first shape to clipper path (subject)
    auto firstPath = shapes[0]->shape()->path().d;
    PathsD subject = parse_svg_to_clipper(firstPath, shapes[0]->transform());

    // Convert remaining shapes to clipper paths (clip)
    PathsD clip;
    for (size_t i = 1; i < shapes.size(); ++i) {
        auto path = shapes[i]->shape()->path().d;
        auto parsed = parse_svg_to_clipper(path, shapes[i]->transform());
        for (auto& p : parsed) {
            clip.push_back(p);
        }
    }

    // Perform boolean operation
    PathsD result;
    switch (op) {
        case BooleanOp::Union:
            result = Clipper2Lib::Union(subject, clip, Clipper2Lib::FillRule::NonZero);
            break;
        case BooleanOp::Intersect:
            result = Clipper2Lib::Intersect(subject, clip, Clipper2Lib::FillRule::NonZero);
            break;
        case BooleanOp::Subtract:
            result = Clipper2Lib::Difference(subject, clip, Clipper2Lib::FillRule::NonZero);
            break;
        case BooleanOp::Exclude:
            result = Clipper2Lib::Xor(subject, clip, Clipper2Lib::FillRule::NonZero);
            break;
    }

    if (result.empty()) return nullptr;

    // Convert result to SVG path
    std::string pathD = clipper_to_svg_path(result);
    if (pathD.empty()) return nullptr;

    // Create new shape with result
    auto newShape = flex::Shape::create();
    newShape->set_path(pathD);

    // Copy fill from first shape
    if (shapes[0]->shape()->has_fill()) {
        auto fill = shapes[0]->shape()->fill();
        newShape->set_fill(fill.color);
    }

    // Copy stroke from first shape
    if (shapes[0]->shape()->has_stroke()) {
        auto stroke = shapes[0]->shape()->stroke();
        newShape->set_stroke(stroke.color, stroke.width);
    }

    auto node = ShapeNode::create(newShape);
    node->setName(std::string(boolean_op_name(op)) + " Result");

    return node;
}

// ============================================================================
// BooleanCommand Implementation
// ============================================================================

BooleanCommand::BooleanCommand(Document* doc, Layer* layer,
                               std::vector<ShapeNode::Ptr> shapes, BooleanOp op)
    : document_(doc)
    , layer_(layer)
    , original_shapes_(std::move(shapes))
    , op_(op) {
}

void BooleanCommand::execute() {
    if (executed_) return;

    // Perform boolean operation
    result_shape_ = perform_boolean_operation(original_shapes_, op_);
    if (!result_shape_) return;

    // Remove original shapes from layer
    for (auto& shape : original_shapes_) {
        layer_->removeNode(shape);
    }

    // Add result shape to layer
    layer_->addNode(result_shape_);

    executed_ = true;
}

void BooleanCommand::undo() {
    if (!executed_) return;

    // Remove result shape
    if (result_shape_) {
        layer_->removeNode(result_shape_);
    }

    // Restore original shapes
    for (auto& shape : original_shapes_) {
        layer_->addNode(shape);
    }

    executed_ = false;
}

std::string BooleanCommand::description() const {
    return std::string("Boolean ") + boolean_op_name(op_);
}

} // namespace editor
