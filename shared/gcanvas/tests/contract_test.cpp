#include "gcanvas/context.hpp"
#include "gcanvas/path.hpp"
#include "gcanvas/window.hpp"

#include <cmath>
#include <type_traits>
#include <utility>

template <typename T, typename = void>
struct has_destroy : std::false_type
{
};

template <typename T>
struct has_destroy<T, std::void_t<decltype(std::declval<T&>().destroy())>> : std::true_type
{
};

static_assert(std::is_abstract_v<gcanvas::Context>);
static_assert(std::has_virtual_destructor_v<gcanvas::Context>);
static_assert(!std::is_copy_constructible_v<gcanvas::Context>);
static_assert(!std::is_copy_assignable_v<gcanvas::Context>);
static_assert(std::is_destructible_v<gcanvas::Image>);
static_assert(std::is_destructible_v<gcanvas::Font>);
static_assert(!has_destroy<gcanvas::Context>::value);
static_assert(!has_destroy<gcanvas::Image>::value);
static_assert(!has_destroy<gcanvas::Font>::value);
static_assert(!has_destroy<gcanvas::Window>::value);
static_assert(
    std::is_same_v<decltype(gcanvas::Window::create(std::declval<gcanvas::WindowConfig>())),
                   std::unique_ptr<gcanvas::Window>>);
static_assert(std::is_same_v<decltype(std::declval<gcanvas::Window&>().subscribe_focus_listener(
                                 std::declval<std::function<void(gcanvas::focus_event)>>())),
                             gcanvas::WindowListenerSubscription>);
static_assert(std::is_same_v<decltype(std::declval<gcanvas::Window&>().subscribe_close_listener(
                                 std::declval<std::function<void(gcanvas::close_event)>>())),
                             gcanvas::WindowListenerSubscription>);
static_assert(!std::is_copy_constructible_v<gcanvas::WindowListenerSubscription>);
static_assert(std::is_nothrow_move_constructible_v<gcanvas::WindowListenerSubscription>);
static_assert(std::is_same_v<
              decltype(std::declval<const gcanvas::Window&>().supports_pointer_capture()), bool>);
static_assert(
    std::is_same_v<decltype(std::declval<const gcanvas::Window&>().has_pointer_capture()), bool>);
static_assert(std::is_same_v<decltype(std::declval<gcanvas::Window&>().get_content_scale()),
                             gcanvas::vec2>);

int main()
{
    gcanvas::WindowConfig config{};
    gcanvas::ResourceLimits limits{};
    if (config.backend != gcanvas::Backend::OpenGL)
    {
        return 1;
    }
    if (gcanvas::Backend::OpenGL == gcanvas::Backend::OpenGLES ||
        gcanvas::Backend::OpenGL == gcanvas::Backend::Vulkan ||
        gcanvas::Backend::OpenGLES == gcanvas::Backend::Vulkan)
    {
        return 2;
    }
    if (limits.max_images != 256 || limits.max_fonts != 32 || limits.max_path_surfaces != 16 ||
        limits.max_path_surface_pixels != 4096U * 4096U || limits.max_path_mesh_quads != 262144U ||
        limits.path_paint_texture_size != 256U || limits.max_clip_vertices != 64U ||
        limits.max_shadow_samples != 25U || limits.max_blur_samples != 25U ||
        limits.max_blur_commands != 65536U)
    {
        return 3;
    }
    const gcanvas::Path path =
        gcanvas::Path::from_svg("M1 2 L3 4 h5 v6 q1 2 3 4 c1 2 3 4 5 6 a4 3 30 0 1 10 8 z");
    if (path.empty() || path.commands().size() < 8U)
    {
        return 4;
    }
    const gcanvas::Transform transform = gcanvas::Transform::translation(4.0f, 5.0f) *
                                         gcanvas::Transform::rotation(0.5f) *
                                         gcanvas::Transform::scaling(2.0f, 3.0f);
    gcanvas::Transform inverse;
    if (!transform.invert(inverse))
    {
        return 5;
    }
    float x = 0.0f, y = 0.0f, restored_x = 0.0f, restored_y = 0.0f;
    transform.map(7.0f, 9.0f, x, y);
    inverse.map(x, y, restored_x, restored_y);
    if (std::fabs(restored_x - 7.0f) > 0.001f || std::fabs(restored_y - 9.0f) > 0.001f)
    {
        return 6;
    }
    const auto gradient = gcanvas::Paint::linear_gradient(
        0.0f, 0.0f, 10.0f, 0.0f,
        {{1.0f, gcanvas::color(0, 0, 255)}, {0.0f, gcanvas::color(255, 0, 0)}});
    if (gradient.stops().front().offset != 0.0f || gradient.stops().back().offset != 1.0f)
    {
        return 7;
    }
    return 0;
}
