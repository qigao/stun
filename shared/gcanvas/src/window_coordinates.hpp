#ifndef GCANVAS_WINDOW_COORDINATES_HPP
#define GCANVAS_WINDOW_COORDINATES_HPP

#include <stdexcept>

namespace gcanvas::detail
{
    struct WindowCoordinateScale
    {
        double x;
        double y;
    };

    struct LogicalPointerPosition
    {
        double x;
        double y;
    };

    inline WindowCoordinateScale window_to_logical_scale(
        int logical_width, int logical_height, int window_width, int window_height)
    {
        if (logical_width <= 0 || logical_height <= 0)
        {
            throw std::logic_error("gCanvas logical window extent must be positive");
        }
        if (window_width <= 0 || window_height <= 0)
        {
            throw std::runtime_error("GLFW returned an empty window coordinate extent");
        }

        return {static_cast<double>(logical_width) / window_width,
                static_cast<double>(logical_height) / window_height};
    }

    inline LogicalPointerPosition map_window_position_to_logical(
        double x, double y, int logical_width, int logical_height,
        int window_width, int window_height)
    {
        const WindowCoordinateScale scale = window_to_logical_scale(
            logical_width, logical_height, window_width, window_height);
        return {x * scale.x, y * scale.y};
    }
}

#endif // GCANVAS_WINDOW_COORDINATES_HPP
