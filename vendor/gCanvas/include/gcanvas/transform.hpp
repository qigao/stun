#ifndef GCANVAS_TRANSFORM_HPP
#define GCANVAS_TRANSFORM_HPP

#include "gcanvas/gcanvas.hpp"

namespace gcanvas
{
    struct GCANVAS_API Transform
    {
        float a = 1.0f;
        float b = 0.0f;
        float c = 0.0f;
        float d = 1.0f;
        float e = 0.0f;
        float f = 0.0f;

        static Transform translation(float x, float y);
        static Transform scaling(float x, float y);
        static Transform rotation(float radians);

        Transform operator*(const Transform& rhs) const;
        bool invert(Transform& result) const noexcept;
        void map(float x, float y, float& mapped_x, float& mapped_y) const noexcept;
    };
}

#endif // GCANVAS_TRANSFORM_HPP
