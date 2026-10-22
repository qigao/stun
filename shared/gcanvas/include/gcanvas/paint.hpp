#ifndef GCANVAS_PAINT_HPP
#define GCANVAS_PAINT_HPP

#include <cstddef>
#include <vector>

#include "gcanvas/color.hpp"
#include "gcanvas/gcanvas.hpp"
#include "gcanvas/transform.hpp"

namespace gcanvas
{
    class Image;

    struct ColorStop
    {
        float offset = 0.0f;
        color value;
    };

    class GCANVAS_API Paint
    {
    public:
        enum class Type
        {
            None,
            Solid,
            LinearGradient,
            RadialGradient,
            ImagePattern
        };

        static constexpr std::size_t max_color_stops = 64;

        static Paint none();
        static Paint solid(color value);
        static Paint linear_gradient(float start_x, float start_y, float end_x, float end_y,
                                     std::vector<ColorStop> stops);
        static Paint radial_gradient(float center_x, float center_y, float inner_radius,
                                     float outer_radius, std::vector<ColorStop> stops);
        static Paint image_pattern(Image& image, float x, float y, float width, float height,
                                   float alpha = 1.0f);

        Type type() const noexcept { return type_; }
        const color& solid_color() const noexcept { return color_; }
        const std::vector<ColorStop>& stops() const noexcept { return stops_; }
        const Transform& paint_transform() const noexcept { return transform_; }
        float inner_radius() const noexcept { return inner_radius_; }
        float outer_radius() const noexcept { return outer_radius_; }
        float pattern_alpha() const noexcept { return alpha_; }
        Image* image() const noexcept { return image_; }

    private:
        Type type_ = Type::None;
        color color_{0, 0, 0, 0};
        std::vector<ColorStop> stops_;
        Transform transform_;
        float inner_radius_ = 0.0f;
        float outer_radius_ = 1.0f;
        float alpha_ = 1.0f;
        Image* image_ = nullptr;
    };
}

#endif // GCANVAS_PAINT_HPP
