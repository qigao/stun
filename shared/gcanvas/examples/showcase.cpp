#include <gcanvas/window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr int showcase_width = 960;
    constexpr int showcase_height = 640;
    constexpr int checker_size = 8;
    constexpr float pi = 3.14159265358979323846f;

    std::array<std::uint8_t, checker_size * checker_size * 4> make_checkerboard()
    {
        std::array<std::uint8_t, checker_size * checker_size * 4> pixels{};
        for (int y = 0; y < checker_size; ++y)
        {
            for (int x = 0; x < checker_size; ++x)
            {
                const bool light = ((x / 2) + (y / 2)) % 2 == 0;
                const std::size_t offset = static_cast<std::size_t>(y * checker_size + x) * 4U;
                pixels[offset] = light ? 255 : 65;
                pixels[offset + 1U] = light ? 184 : 90;
                pixels[offset + 2U] = light ? 77 : 210;
                pixels[offset + 3U] = 255;
            }
        }
        return pixels;
    }

    struct Options
    {
        gcanvas::Backend backend = gcanvas::Backend::OpenGL;
        bool smoke = false;
    };

    Options parse_options(int argc, char** argv)
    {
        Options options;
        bool backend_selected = false;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (!backend_selected && argument == "opengl")
            {
                options.backend = gcanvas::Backend::OpenGL;
                backend_selected = true;
            }
            else if (!backend_selected && argument == "vulkan")
            {
                options.backend = gcanvas::Backend::Vulkan;
                backend_selected = true;
            }
            else if (argument == "--smoke")
            {
                options.smoke = true;
            }
            else
            {
                throw std::invalid_argument(
                    "usage: gcanvas_showcase [opengl|vulkan] [--smoke]");
            }
        }
        return options;
    }

    void draw_scene(gcanvas::Context& canvas, gcanvas::Image& checker, float seconds)
    {
        const float width = static_cast<float>(canvas.get_width());
        const float height = static_cast<float>(canvas.get_height());
        const float margin = 32.0f;
        const float content_width = (std::max)(320.0f, width - margin * 2.0f);

        canvas.set_clear_color(gcanvas::color("#0b1020"));

        canvas.set_fill_color(gcanvas::color(18, 29, 54, 232));
        canvas.fill_rounded_rect(margin, margin, content_width, 88.0f, 20.0f);
        canvas.set_fill_color(gcanvas::color(244, 247, 255, 230));
        canvas.set_font_size(30);
        canvas.draw_text(margin + 24.0f, margin + 12.0f, "gCanvas GPU Showcase");
        canvas.set_fill_color(gcanvas::color(164, 180, 214, 178));
        canvas.set_font_size(15);
        canvas.draw_text(margin + 26.0f, margin + 54.0f,
                         "one canvas contract  |  OpenGL + Vulkan  |  native GPU composition");

        const float card_y = 144.0f;
        const float card_height = (std::max)(180.0f, height - card_y - margin);
        const float gap = 20.0f;
        const float left_width = content_width * 0.58f;
        const float right_x = margin + left_width + gap;
        const float right_width = content_width - left_width - gap;

        canvas.set_fill_color(gcanvas::color(0, 0, 0, 118));
        canvas.draw_rounded_rect_shadow(margin + 3.0f, card_y + 8.0f, left_width,
                                        card_height, 24.0f, 18.0f);
        canvas.draw_rounded_rect_shadow(right_x + 3.0f, card_y + 8.0f, right_width,
                                        card_height, 24.0f, 18.0f);

        gcanvas::Path left_card;
        left_card.rounded_rect(margin, card_y, left_width, card_height, 24.0f);
        canvas.fill_path(
            left_card,
            gcanvas::Paint::linear_gradient(
                margin, card_y, margin + left_width, card_y + card_height,
                {{0.0f, gcanvas::color(47, 92, 180, 245)},
                 {0.48f, gcanvas::color(109, 71, 184, 232)},
                 {1.0f, gcanvas::color(232, 91, 123, 220)}}));

        canvas.set_rect_mask(margin + 12.0f, card_y + 12.0f, left_width - 24.0f,
                             card_height - 24.0f);
        for (int index = 0; index < 5; ++index)
        {
            const float phase = seconds * (0.55f + index * 0.08f) + index * 0.9f;
            const float x = margin + left_width * (0.18f + index * 0.16f) +
                            std::sin(phase) * 24.0f;
            const float y = card_y + card_height * 0.56f + std::cos(phase * 1.3f) * 52.0f;
            canvas.set_fill_color(gcanvas::color(255, 255, 255, 46 + index * 18));
            canvas.fill_circle(x, y, 28.0f + index * 7.0f);
        }
        canvas.remove_rect_mask();

        const gcanvas::Path mark = gcanvas::Path::from_svg(
            "M50 4 L61 36 L95 36 L68 56 L78 89 L50 69 L22 89 L32 56 L5 36 L39 36 Z");
        const gcanvas::Transform mark_transform =
            gcanvas::Transform::translation(margin + left_width * 0.5f,
                                            card_y + card_height * 0.48f) *
            gcanvas::Transform::rotation(seconds * 0.35f) *
            gcanvas::Transform::translation(-50.0f, -50.0f);
        canvas.fill_path(
            mark,
            gcanvas::Paint::radial_gradient(
                50.0f, 50.0f, 4.0f, 52.0f,
                {{0.0f, gcanvas::color(255, 255, 255, 244)},
                 {1.0f, gcanvas::color(255, 204, 93, 176)}}),
            mark_transform);
        canvas.stroke_path(mark, gcanvas::Paint::solid(gcanvas::color(255, 255, 255, 128)),
                           2.5f, mark_transform);

        canvas.set_fill_color(gcanvas::color(18, 29, 54, 242));
        canvas.fill_rounded_rect(right_x, card_y, right_width, card_height, 24.0f);
        canvas.set_fill_color(gcanvas::color(255, 255, 255, 150));
        canvas.draw_image(right_x + 24.0f, card_y + 24.0f,
                          (std::min)(120.0f, right_width - 48.0f), 120.0f, checker, true);

        canvas.set_fill_color(gcanvas::color(239, 243, 255, 230));
        canvas.set_font_size(21);
        canvas.draw_text(right_x + 24.0f, card_y + 166.0f, "Composed on GPU");
        canvas.set_fill_color(gcanvas::color(157, 174, 209, 176));
        canvas.set_font_size(14);
        canvas.draw_text(right_x + 24.0f, card_y + 202.0f,
                         "paths + gradients\nanalytic GPU shadows\nstencil clipping\nshared backend semantics");

        const float pulse = 0.5f + 0.5f * std::sin(seconds * pi);
        canvas.set_fill_color(gcanvas::color(79, 220, 168, static_cast<int>(110 + pulse * 100)));
        canvas.fill_rounded_rect(right_x + 24.0f, card_y + card_height - 54.0f,
                                 (std::max)(40.0f, right_width - 48.0f), 30.0f, 15.0f);
    }
}

int main(int argc, char** argv)
{
    try
    {
        const Options options = parse_options(argc, argv);
        gcanvas::WindowConfig config{"gCanvas Showcase", showcase_width, showcase_height};
        config.backend = options.backend;
        config.visible = !options.smoke;
        config.vsync = !options.smoke;

        auto window = gcanvas::Window::create(config);
        gcanvas::Context& canvas = window->create_context();
        const auto checker_pixels = make_checkerboard();
        gcanvas::Image& checker = canvas.create_image(
            checker_size, checker_size, 4, checker_pixels.data(), checker_pixels.size());

        const double started = gcanvas::Window::now();
        int frames = 0;
        while (window->is_running() && (!options.smoke || frames < 2))
        {
            window->poll_events();
            draw_scene(canvas, checker,
                       static_cast<float>(gcanvas::Window::now() - started));
            canvas.draw_frame();
            canvas.present_frame();
            ++frames;
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "gCanvas showcase failed: " << error.what() << '\n';
        return 1;
    }
}
