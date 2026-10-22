#ifndef GCANVAS_VERTEX_HPP
#define GCANVAS_VERTEX_HPP

#include <array>

#include "gcanvas/color.hpp"
#include "gcanvas/vec2.hpp"
#include <vulkan/vulkan.h>

namespace gcanvas
{
    class point_vertex
    {
    public:
        vec2 position;
        // vec2 uv;
        // color fill_color;
        // float border_radius;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 1> gerAttributeDescriptions();
    };

} // namespace gcanvas

#endif // GCANVAS_VERTEX_HPP
