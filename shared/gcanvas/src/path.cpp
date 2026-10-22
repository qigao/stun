#include "gcanvas/path.hpp"

#include "gcanvas/image.hpp"
#include "gcanvas/paint.hpp"
#include "gcanvas/transform.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace gcanvas
{
    namespace
    {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float determinant_epsilon = 1.0e-8f;

        void require_finite(float value, const char* field)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument(std::string("gCanvas path ") + field +
                                            " must be finite");
        }

        std::vector<ColorStop> normalize_stops(std::vector<ColorStop> stops)
        {
            if (stops.size() < 2U || stops.size() > Paint::max_color_stops)
                throw std::invalid_argument("gCanvas gradients require 2 to 64 color stops");
            for (const ColorStop& stop : stops)
                if (!std::isfinite(stop.offset) || stop.offset < 0.0f || stop.offset > 1.0f)
                    throw std::invalid_argument("gCanvas gradient stop offset must be in [0, 1]");
            std::stable_sort(stops.begin(), stops.end(),
                             [](const ColorStop& lhs, const ColorStop& rhs) {
                                 return lhs.offset < rhs.offset;
                             });
            return stops;
        }

        void append_arc(Path& path, float x1, float y1, float x2, float y2, float radius_x,
                        float radius_y, float rotation_degrees, bool large_arc, bool sweep)
        {
            radius_x = std::fabs(radius_x);
            radius_y = std::fabs(radius_y);
            if (radius_x <= determinant_epsilon || radius_y <= determinant_epsilon ||
                (std::fabs(x1 - x2) <= determinant_epsilon &&
                 std::fabs(y1 - y2) <= determinant_epsilon))
            {
                path.line_to(x2, y2);
                return;
            }

            const float phi = rotation_degrees * pi / 180.0f;
            const float cos_phi = std::cos(phi);
            const float sin_phi = std::sin(phi);
            const float dx = (x1 - x2) * 0.5f;
            const float dy = (y1 - y2) * 0.5f;
            const float x1p = cos_phi * dx + sin_phi * dy;
            const float y1p = -sin_phi * dx + cos_phi * dy;

            float rx2 = radius_x * radius_x;
            float ry2 = radius_y * radius_y;
            const float scale = x1p * x1p / rx2 + y1p * y1p / ry2;
            if (scale > 1.0f)
            {
                const float factor = std::sqrt(scale);
                radius_x *= factor;
                radius_y *= factor;
                rx2 = radius_x * radius_x;
                ry2 = radius_y * radius_y;
            }

            const float numerator = (std::max)(0.0f, rx2 * ry2 - rx2 * y1p * y1p -
                                                         ry2 * x1p * x1p);
            const float denominator = rx2 * y1p * y1p + ry2 * x1p * x1p;
            float coefficient = denominator <= determinant_epsilon
                                    ? 0.0f
                                    : std::sqrt(numerator / denominator);
            if (large_arc == sweep)
                coefficient = -coefficient;

            const float cxp = coefficient * radius_x * y1p / radius_y;
            const float cyp = coefficient * -radius_y * x1p / radius_x;
            const float center_x = cos_phi * cxp - sin_phi * cyp + (x1 + x2) * 0.5f;
            const float center_y = sin_phi * cxp + cos_phi * cyp + (y1 + y2) * 0.5f;

            const auto angle = [](float ux, float uy, float vx, float vy) {
                return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
            };
            const float ux = (x1p - cxp) / radius_x;
            const float uy = (y1p - cyp) / radius_y;
            const float vx = (-x1p - cxp) / radius_x;
            const float vy = (-y1p - cyp) / radius_y;
            float theta = std::atan2(uy, ux);
            float delta = angle(ux, uy, vx, vy);
            if (!sweep && delta > 0.0f)
                delta -= 2.0f * pi;
            else if (sweep && delta < 0.0f)
                delta += 2.0f * pi;

            const int segments = (std::max)(1, static_cast<int>(std::ceil(std::fabs(delta) /
                                                                          (pi * 0.5f))));
            const float step = delta / static_cast<float>(segments);
            for (int segment = 0; segment < segments; ++segment)
            {
                const float start = theta + step * static_cast<float>(segment);
                const float end = start + step;
                const float alpha = 4.0f / 3.0f * std::tan((end - start) * 0.25f);
                const float cos_start = std::cos(start);
                const float sin_start = std::sin(start);
                const float cos_end = std::cos(end);
                const float sin_end = std::sin(end);

                const auto map = [&](float x, float y, float& out_x, float& out_y) {
                    out_x = center_x + cos_phi * radius_x * x - sin_phi * radius_y * y;
                    out_y = center_y + sin_phi * radius_x * x + cos_phi * radius_y * y;
                };
                float c1x = 0.0f, c1y = 0.0f, c2x = 0.0f, c2y = 0.0f;
                float ex = 0.0f, ey = 0.0f;
                map(cos_start - alpha * sin_start, sin_start + alpha * cos_start, c1x, c1y);
                map(cos_end + alpha * sin_end, sin_end - alpha * cos_end, c2x, c2y);
                map(cos_end, sin_end, ex, ey);
                path.cubic_to(c1x, c1y, c2x, c2y, ex, ey);
            }
        }
    }

    Transform Transform::translation(float x, float y)
    {
        require_finite(x, "translation x");
        require_finite(y, "translation y");
        Transform result;
        result.e = x;
        result.f = y;
        return result;
    }

    Transform Transform::scaling(float x, float y)
    {
        require_finite(x, "scale x");
        require_finite(y, "scale y");
        Transform result;
        result.a = x;
        result.d = y;
        return result;
    }

    Transform Transform::rotation(float radians)
    {
        require_finite(radians, "rotation");
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Transform{cosine, sine, -sine, cosine, 0.0f, 0.0f};
    }

    Transform Transform::operator*(const Transform& rhs) const
    {
        return Transform{a * rhs.a + c * rhs.b, b * rhs.a + d * rhs.b,
                         a * rhs.c + c * rhs.d, b * rhs.c + d * rhs.d,
                         a * rhs.e + c * rhs.f + e, b * rhs.e + d * rhs.f + f};
    }

    bool Transform::invert(Transform& result) const noexcept
    {
        const float determinant = a * d - b * c;
        if (!std::isfinite(determinant) || std::fabs(determinant) <= determinant_epsilon)
            return false;
        const float inverse = 1.0f / determinant;
        result = Transform{d * inverse, -b * inverse, -c * inverse, a * inverse,
                           (c * f - d * e) * inverse, (b * e - a * f) * inverse};
        return true;
    }

    void Transform::map(float x, float y, float& mapped_x, float& mapped_y) const noexcept
    {
        mapped_x = a * x + c * y + e;
        mapped_y = b * x + d * y + f;
    }

    Paint Paint::none() { return Paint{}; }

    Paint Paint::solid(color value)
    {
        Paint result;
        result.type_ = Type::Solid;
        result.color_ = value;
        return result;
    }

    Paint Paint::linear_gradient(float start_x, float start_y, float end_x, float end_y,
                                 std::vector<ColorStop> stops)
    {
        require_finite(start_x, "linear gradient start x");
        require_finite(start_y, "linear gradient start y");
        require_finite(end_x, "linear gradient end x");
        require_finite(end_y, "linear gradient end y");
        if (std::fabs(end_x - start_x) <= determinant_epsilon &&
            std::fabs(end_y - start_y) <= determinant_epsilon)
            throw std::invalid_argument("gCanvas linear gradient endpoints must differ");
        Paint result;
        result.type_ = Type::LinearGradient;
        result.transform_ = Transform{end_x - start_x, end_y - start_y,
                                      -(end_y - start_y), end_x - start_x, start_x, start_y};
        result.stops_ = normalize_stops(std::move(stops));
        return result;
    }

    Paint Paint::radial_gradient(float center_x, float center_y, float inner_radius,
                                 float outer_radius, std::vector<ColorStop> stops)
    {
        require_finite(center_x, "radial gradient center x");
        require_finite(center_y, "radial gradient center y");
        if (!std::isfinite(inner_radius) || !std::isfinite(outer_radius) || inner_radius < 0.0f ||
            outer_radius <= inner_radius)
            throw std::invalid_argument("gCanvas radial gradient radii are invalid");
        Paint result;
        result.type_ = Type::RadialGradient;
        result.transform_ = Transform::translation(center_x, center_y);
        result.inner_radius_ = inner_radius;
        result.outer_radius_ = outer_radius;
        result.stops_ = normalize_stops(std::move(stops));
        return result;
    }

    Paint Paint::image_pattern(Image& image, float x, float y, float width, float height,
                               float alpha)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
            !std::isfinite(height) || width <= 0.0f || height <= 0.0f ||
            !std::isfinite(alpha) || alpha < 0.0f || alpha > 1.0f)
            throw std::invalid_argument("gCanvas image pattern dimensions or alpha are invalid");
        Paint result;
        result.type_ = Type::ImagePattern;
        result.image_ = &image;
        result.alpha_ = alpha;
        result.transform_ = Transform{width, 0.0f, 0.0f, height, x, y};
        return result;
    }

    void Path::append(Command command)
    {
        if (commands_.size() >= max_commands)
            throw std::length_error("gCanvas path command limit reached");
        commands_.push_back(command);
    }

    Path& Path::move_to(float x, float y)
    {
        require_finite(x, "x"); require_finite(y, "y");
        append(Command{CommandType::MoveTo, {x, y}});
        current_x_ = subpath_x_ = x;
        current_y_ = subpath_y_ = y;
        has_current_point_ = true;
        return *this;
    }

    Path& Path::line_to(float x, float y)
    {
        if (!has_current_point_)
            throw std::logic_error("gCanvas line_to requires a current point");
        require_finite(x, "x"); require_finite(y, "y");
        append(Command{CommandType::LineTo, {x, y}});
        current_x_ = x; current_y_ = y;
        return *this;
    }

    Path& Path::cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y)
    {
        if (!has_current_point_)
            throw std::logic_error("gCanvas cubic_to requires a current point");
        for (float value : {c1x, c1y, c2x, c2y, x, y}) require_finite(value, "coordinate");
        append(Command{CommandType::CubicTo, {c1x, c1y, c2x, c2y, x, y}});
        current_x_ = x; current_y_ = y;
        return *this;
    }

    Path& Path::quadratic_to(float control_x, float control_y, float x, float y)
    {
        if (!has_current_point_)
            throw std::logic_error("gCanvas quadratic_to requires a current point");
        const float c1x = current_x_ + (control_x - current_x_) * (2.0f / 3.0f);
        const float c1y = current_y_ + (control_y - current_y_) * (2.0f / 3.0f);
        const float c2x = x + (control_x - x) * (2.0f / 3.0f);
        const float c2y = y + (control_y - y) * (2.0f / 3.0f);
        return cubic_to(c1x, c1y, c2x, c2y, x, y);
    }

    Path& Path::close()
    {
        if (!has_current_point_)
            throw std::logic_error("gCanvas close requires a current subpath");
        append(Command{CommandType::Close, {}});
        current_x_ = subpath_x_; current_y_ = subpath_y_;
        return *this;
    }

    Path& Path::rect(float x, float y, float width, float height)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0.0f || height < 0.0f)
            throw std::invalid_argument("gCanvas rectangle dimensions must be non-negative");
        return move_to(x, y).line_to(x + width, y).line_to(x + width, y + height)
                            .line_to(x, y + height).close();
    }

    Path& Path::rounded_rect(float x, float y, float width, float height, float radius)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(radius) ||
            width < 0.0f || height < 0.0f || radius < 0.0f)
            throw std::invalid_argument("gCanvas rounded rectangle dimensions are invalid");
        radius = (std::min)(radius, (std::min)(width, height) * 0.5f);
        if (radius == 0.0f) return rect(x, y, width, height);
        constexpr float kappa = 0.5522847498307936f;
        move_to(x + radius, y).line_to(x + width - radius, y);
        cubic_to(x + width - radius + kappa * radius, y, x + width,
                 y + radius - kappa * radius, x + width, y + radius);
        line_to(x + width, y + height - radius);
        cubic_to(x + width, y + height - radius + kappa * radius,
                 x + width - radius + kappa * radius, y + height,
                 x + width - radius, y + height);
        line_to(x + radius, y + height);
        cubic_to(x + radius - kappa * radius, y + height, x,
                 y + height - radius + kappa * radius, x, y + height - radius);
        line_to(x, y + radius);
        cubic_to(x, y + radius - kappa * radius, x + radius - kappa * radius, y,
                 x + radius, y);
        return close();
    }

    Path& Path::ellipse(float cx, float cy, float rx, float ry)
    {
        if (!std::isfinite(rx) || !std::isfinite(ry) || rx < 0.0f || ry < 0.0f)
            throw std::invalid_argument("gCanvas ellipse radii must be non-negative");
        constexpr float kappa = 0.5522847498307936f;
        move_to(cx + rx, cy);
        cubic_to(cx + rx, cy + kappa * ry, cx + kappa * rx, cy + ry, cx, cy + ry);
        cubic_to(cx - kappa * rx, cy + ry, cx - rx, cy + kappa * ry, cx - rx, cy);
        cubic_to(cx - rx, cy - kappa * ry, cx - kappa * rx, cy - ry, cx, cy - ry);
        cubic_to(cx + kappa * rx, cy - ry, cx + rx, cy - kappa * ry, cx + rx, cy);
        return close();
    }

    void Path::clear() noexcept
    {
        commands_.clear();
        current_x_ = current_y_ = subpath_x_ = subpath_y_ = 0.0f;
        has_current_point_ = false;
    }

    Path Path::from_svg(std::string_view view)
    {
        constexpr std::size_t svg_bytes_per_command_hint = 8U;
        const std::string data(view);
        Path path;
        path.commands_.reserve(
            std::min(max_commands, view.size() / svg_bytes_per_command_hint + 1U));
        std::size_t index = 0;
        char command = 0;
        float current_x = 0.0f, current_y = 0.0f, start_x = 0.0f, start_y = 0.0f;
        const auto skip = [&]() {
            while (index < data.size() && (std::isspace(static_cast<unsigned char>(data[index])) ||
                                            data[index] == ',')) ++index;
        };
        const auto number = [&]() {
            skip();
            if (index >= data.size()) throw std::invalid_argument("gCanvas SVG path is truncated");
            char* end = nullptr;
            const float value = std::strtof(data.c_str() + index, &end);
            if (end == data.c_str() + index || !std::isfinite(value))
                throw std::invalid_argument("gCanvas SVG path contains an invalid number");
            index = static_cast<std::size_t>(end - data.c_str());
            return value;
        };

        while (true)
        {
            skip();
            if (index >= data.size()) break;
            if (std::isalpha(static_cast<unsigned char>(data[index]))) command = data[index++];
            else if (command == 0) throw std::invalid_argument("gCanvas SVG path requires a command");
            const bool relative = command >= 'a' && command <= 'z';
            const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
            if (upper == 'Z')
            {
                path.close(); current_x = start_x; current_y = start_y; command = 0; continue;
            }
            if (upper == 'M')
            {
                float x = number(), y = number();
                if (relative) { x += current_x; y += current_y; }
                path.move_to(x, y); current_x = start_x = x; current_y = start_y = y;
                command = relative ? 'l' : 'L'; continue;
            }
            if (upper == 'L')
            {
                float x = number(), y = number();
                if (relative) { x += current_x; y += current_y; }
                path.line_to(x, y); current_x = x; current_y = y; continue;
            }
            if (upper == 'H')
            {
                float x = number(); if (relative) x += current_x;
                path.line_to(x, current_y); current_x = x; continue;
            }
            if (upper == 'V')
            {
                float y = number(); if (relative) y += current_y;
                path.line_to(current_x, y); current_y = y; continue;
            }
            if (upper == 'C')
            {
                float c1x = number(), c1y = number(), c2x = number(), c2y = number();
                float x = number(), y = number();
                if (relative) { c1x += current_x; c1y += current_y; c2x += current_x;
                                c2y += current_y; x += current_x; y += current_y; }
                path.cubic_to(c1x, c1y, c2x, c2y, x, y); current_x = x; current_y = y; continue;
            }
            if (upper == 'Q')
            {
                float cx = number(), cy = number(), x = number(), y = number();
                if (relative) { cx += current_x; cy += current_y; x += current_x; y += current_y; }
                path.quadratic_to(cx, cy, x, y); current_x = x; current_y = y; continue;
            }
            if (upper == 'A')
            {
                const float rx = number(), ry = number(), rotation = number();
                const bool large = number() != 0.0f, sweep = number() != 0.0f;
                float x = number(), y = number();
                if (relative) { x += current_x; y += current_y; }
                append_arc(path, current_x, current_y, x, y, rx, ry, rotation, large, sweep);
                current_x = x; current_y = y; continue;
            }
            throw std::invalid_argument(std::string("gCanvas SVG path command is unsupported: ") +
                                        upper);
        }
        return path;
    }
}
