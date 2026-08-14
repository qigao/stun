#include <gcanvas/backends/opengl.hpp>
#include <gcanvas/backends/vulkan.hpp>
#include <gcanvas/context.hpp>
#include <gcanvas/paint.hpp>
#include <gcanvas/path.hpp>
#include <gcanvas/svg.hpp>
#include <gcanvas/transform.hpp>
#include <gcanvas/window.hpp>

#include <type_traits>

int main()
{
    static_assert(std::is_abstract_v<gcanvas::Context>);
    gcanvas::WindowConfig config{"install-consumer", 64, 64};
    config.backend = gcanvas::Backend::OpenGL;
    gcanvas::Path path;
    path.move_to(0.0f, 0.0f).quadratic_to(4.0f, 8.0f, 8.0f, 0.0f);
    const auto paint = gcanvas::Paint::solid(gcanvas::color(255, 255, 255, 255));
    const auto transform = gcanvas::Transform::translation(2.0f, 3.0f);
    auto svg_document = gcanvas::SvgDocument::load_data(
        "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'/>");
    float x = 0.0f;
    float y = 0.0f;
    transform.map(1.0f, 1.0f, x, y);
    return config.backend == gcanvas::Backend::OpenGL && !path.empty() &&
                   paint.type() == gcanvas::Paint::Type::Solid && svg_document &&
                   x == 3.0f && y == 4.0f
               ? 0
               : 1;
}
