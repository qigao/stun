// Curve interpolation adapted from d3-shape 3.2.0 (ISC):
// Copyright 2010-2022 Mike Bostock
//
// Permission to use, copy, modify, and/or distribute this software for any purpose
// with or without fee is hereby granted, provided that the above copyright notice
// and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
// THIS SOFTWARE.

#include "flowchart_curve.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mermaid::flowchart::curve {
namespace {
Point mid(Point a, Point b) { return {a.x / 2 + b.x / 2, a.y / 2 + b.y / 2}; }
Point add(Point a, Point b, double scale) { return {a.x + b.x * scale, a.y + b.y * scale}; }
Point sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
bool same(Point a, Point b) { return a.x == b.x && a.y == b.y; }
bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
void line(std::vector<Command>& out, Point p) { out.push_back({'L', {p}}); }
void cubic(std::vector<Command>& out, Point a, Point b, Point p) { out.push_back({'C', {a, b, p}}); }

std::vector<std::array<double, 2>> natural_controls(const std::vector<double>& x) {
    const auto n = x.size() - 1;
    std::vector<double> a(n), b(n), r(n);
    a[0] = 0; b[0] = 2; r[0] = x[0] + 2 * x[1];
    for (std::size_t i = 1; i + 1 < n; ++i) {
        a[i] = 1; b[i] = 4; r[i] = 4 * x[i] + 2 * x[i + 1];
    }
    a[n - 1] = 2; b[n - 1] = 7; r[n - 1] = 8 * x[n - 1] + x[n];
    for (std::size_t i = 1; i < n; ++i) {
        const auto m = a[i] / b[i - 1];
        b[i] -= m; r[i] -= m * r[i - 1];
    }
    a[n - 1] = r[n - 1] / b[n - 1];
    for (std::size_t i = n - 1; i-- > 0;) a[i] = (r[i] - a[i + 1]) / b[i];
    b[n - 1] = (x[n] + a[n - 1]) / 2;
    for (std::size_t i = 0; i + 1 < n; ++i) b[i] = 2 * x[i + 1] - a[i + 1];
    std::vector<std::array<double, 2>> result(n);
    for (std::size_t i = 0; i < n; ++i) result[i] = {a[i], b[i]};
    return result;
}

double sign(double x) { return x < 0 ? -1 : 1; }
double slope3(Point a, Point b, Point c) {
    const double h0 = b.x - a.x, h1 = c.x - b.x;
    const double s0 = (b.y - a.y) / (h0 ? h0 : h1 < 0 ? -0.0 : 0.0);
    const double s1 = (c.y - b.y) / (h1 ? h1 : h0 < 0 ? -0.0 : 0.0);
    const double p = (s0 * h1 + s1 * h0) / (h0 + h1);
    if (std::isnan(s0) || std::isnan(s1) || std::isnan(p)) return 0;
    const double v = (sign(s0) + sign(s1)) * std::min({std::abs(s0), std::abs(s1), .5 * std::abs(p)});
    return std::isnan(v) || v == 0 ? 0 : v;
}
double slope2(Point a, Point b, double tangent) {
    return b.x != a.x ? (3 * (b.y - a.y) / (b.x - a.x) - tangent) / 2 : tangent;
}

struct Rect { double left, top, right, bottom; };
bool outside(const std::array<Point, 4>& p, const Rect& r) {
    double l = p[0].x, t = p[0].y, h = l, b = t;
    for (const auto v : p) { l = std::min(l, v.x); h = std::max(h, v.x); t = std::min(t, v.y); b = std::max(b, v.y); }
    return h <= r.left || l >= r.right || b <= r.top || t >= r.bottom;
}
bool inside(Point p, const Rect& r) { return p.x > r.left && p.x < r.right && p.y > r.top && p.y < r.bottom; }
bool line_hits(Point a, Point b, const Rect& r) {
    double lo = 0, hi = 1;
    for (int axis = 0; axis != 2; ++axis) {
        const double start = axis ? a.y : a.x, delta = axis ? b.y - a.y : b.x - a.x;
        const double low = axis ? r.top : r.left, high = axis ? r.bottom : r.right;
        if (delta == 0) { if (start <= low || start >= high) return false; }
        else {
            double x = (low - start) / delta, y = (high - start) / delta;
            if (x > y) std::swap(x, y);
            lo = std::max(lo, x); hi = std::min(hi, y);
        }
    }
    return lo < hi;
}
void certify(const std::array<Point, 4>& p, const Rect& r, unsigned depth, std::size_t& budget) {
    if (!budget) throw std::invalid_argument("flowchart: curve obstacle-check budget exhausted");
    --budget;
    if (outside(p, r)) return;
    const auto a = mid(p[0], p[1]), b = mid(p[1], p[2]), c = mid(p[2], p[3]);
    const auto d = mid(a, b), e = mid(b, c), m = mid(d, e);
    if (inside(p[0], r) || inside(p[3], r) || inside(m, r))
        throw std::invalid_argument("flowchart: named curve enters a node obstacle");
    if (!depth) throw std::invalid_argument("flowchart: named curve clearance is not certifiable");
    certify({p[0], a, d, m}, r, depth - 1, budget);
    certify({m, e, c, p[3]}, r, depth - 1, budget);
}
} // namespace

bool supported(std::string_view name) {
    return name.empty() || name == "linear" || name == "natural" || name == "cardinal" ||
        name == "catmullRom" || name == "monotoneX" || name == "monotoneY" ||
        name == "step" || name == "stepBefore" || name == "stepAfter" || name == "bumpX" || name == "bumpY";
}

std::vector<Command> interpolate(const std::vector<Point>& points, std::string_view name) {
    if (!supported(name)) throw std::invalid_argument("flowchart: unsupported named curve");
    for (const auto p : points) if (!finite(p)) throw std::invalid_argument("flowchart: nonfinite curve waypoint");
    std::vector<Command> out;
    if (points.empty()) return out;
    out.push_back({'M', {points[0]}});
    const auto n = points.size();
    if (name.empty() || name == "linear") {
        for (std::size_t i = 1; i < n; ++i) line(out, points[i]);
    } else if (name == "natural") {
        if (n == 2) line(out, points[1]);
        else if (n > 2) {
            std::vector<double> x, y;
            for (auto p : points) { x.push_back(p.x); y.push_back(p.y); }
            const auto cx = natural_controls(x), cy = natural_controls(y);
            for (std::size_t i = 0; i + 1 < n; ++i)
                cubic(out, {cx[i][0], cy[i][0]}, {cx[i][1], cy[i][1]}, points[i + 1]);
        }
    } else if (name == "cardinal" || name == "catmullRom") {
        if (n == 2) line(out, points[1]);
        else for (std::size_t i = 0; i + 1 < n; ++i) {
            const auto p0 = i ? points[i - 1] : points[i + 1];
            const auto p1 = points[i], p2 = points[i + 1];
            const auto p3 = i + 2 < n ? points[i + 2] : points[i];
            Point a = p1, b = p2;
            if (name == "cardinal") {
                a = add(p1, sub(p2, p0), 1.0 / 6);
                b = add(p2, sub(p1, p3), 1.0 / 6);
            } else {
                // Centripetal Catmull-Rom, alpha=0.5. Repeated endpoint knots
                // have zero spacing, matching d3's streaming end conditions.
                const auto before = i ? points[i - 1] : p1;
                const auto after = i + 2 < n ? points[i + 2] : p2;
                const double l01 = std::hypot(p1.x - before.x, p1.y - before.y);
                const double l12 = std::hypot(p2.x - p1.x, p2.y - p1.y);
                const double l23 = std::hypot(after.x - p2.x, after.y - p2.y);
                const double s01 = std::sqrt(l01), s12 = std::sqrt(l12), s23 = std::sqrt(l23);
                if (s01 > 1e-6) {
                    const double k = 2*l01 + 3*s01*s12 + l12, d = 3*s01*(s01+s12);
                    a = {(p1.x*k-before.x*l12+p2.x*l01)/d, (p1.y*k-before.y*l12+p2.y*l01)/d};
                }
                if (s23 > 1e-6) {
                    const double k = 2*l23 + 3*s23*s12 + l12, d = 3*s23*(s23+s12);
                    b = {(p2.x*k+p1.x*l23-after.x*l12)/d, (p2.y*k+p1.y*l23-after.y*l12)/d};
                }
            }
            cubic(out, a, b, p2);
        }
    } else if (name == "monotoneX" || name == "monotoneY") {
        const bool reflect = name == "monotoneY";
        std::vector<Point> p;
        for (auto v : points) {
            if (reflect) std::swap(v.x, v.y);
            if (p.empty() || !same(v, p.back())) p.push_back(v);
        }
        auto emit = [&](Point a, Point b, Point c) {
            if (reflect) { std::swap(a.x, a.y); std::swap(b.x, b.y); std::swap(c.x, c.y); }
            cubic(out, a, b, c);
        };
        if (p.size() == 2) line(out, points.back());
        else if (p.size() > 2) {
            std::vector<double> t(p.size());
            for (std::size_t i = 1; i + 1 < p.size(); ++i) t[i] = slope3(p[i - 1], p[i], p[i + 1]);
            t.front() = slope2(p[0], p[1], t[1]);
            t.back() = slope2(p[p.size() - 2], p.back(), t[p.size() - 2]);
            for (std::size_t i = 0; i + 1 < p.size(); ++i) {
                const auto dx = (p[i + 1].x - p[i].x) / 3;
                emit({p[i].x + dx, p[i].y + dx*t[i]},
                     {p[i + 1].x - dx, p[i + 1].y - dx*t[i + 1]}, p[i + 1]);
            }
        }
        if (p.size() == 1) out.push_back({'Z', {}});
    } else if (name == "bumpX" || name == "bumpY") {
        for (std::size_t i = 1; i < n; ++i) {
            const auto a = points[i - 1], b = points[i], m = mid(a, b);
            if (name == "bumpX") cubic(out, {m.x, a.y}, {m.x, b.y}, b);
            else cubic(out, {a.x, m.y}, {b.x, m.y}, b);
        }
    } else {
        const double t = name == "stepBefore" ? 0 : name == "stepAfter" ? 1 : .5;
        for (std::size_t i = 1; i < n; ++i) {
            const auto a = points[i - 1], b = points[i];
            if (t == 0) { line(out, {a.x, b.y}); line(out, b); }
            else { const double x = a.x*(1-t) + b.x*t; line(out, {x, a.y}); line(out, {x, b.y}); }
        }
        if (t == .5 && n > 1) line(out, points.back());
    }
    if (n == 1 && (out.empty() || out.back().kind != 'Z')) out.push_back({'Z', {}});
    for (const auto& c : out) for (std::size_t i = 0; i < c.size(); ++i)
        if (!finite(c.points[i])) throw std::invalid_argument("flowchart: nonfinite curve control point");
    return out;
}

void check_obstacles(const std::vector<Command>& path, const std::vector<RenderedNode>& nodes) {
    Point current{};
    std::size_t budget = 65536;
    for (const auto& c : path) {
        if (c.kind == 'M') { current = c.points[0]; continue; }
        if (c.kind == 'Z') continue;
        for (const auto& node : nodes) {
            const Rect r{node.x, node.y, node.x + node.width, node.y + node.height};
            if (!std::isfinite(r.left) || !std::isfinite(r.top) || !std::isfinite(r.right) ||
                !std::isfinite(r.bottom) || node.width <= 0 || node.height <= 0)
                throw std::invalid_argument("flowchart: invalid curve obstacle");
            if (c.kind == 'C') certify({current, c.points[0], c.points[1], c.points[2]}, r, 20, budget);
            else {
                if (!budget) throw std::invalid_argument("flowchart: curve obstacle-check budget exhausted");
                --budget;
                if (line_hits(current, c.points[0], r))
                    throw std::invalid_argument("flowchart: named curve enters a node obstacle");
            }
        }
        current = c.points[c.size() - 1];
    }
}
} // namespace mermaid::flowchart::curve
