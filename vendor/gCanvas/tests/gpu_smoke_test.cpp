#include "gcanvas/window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    bool near_channel(std::uint8_t actual, std::uint8_t expected)
    {
        constexpr int tolerance = 8;
        return std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <= tolerance;
    }

    bool verify_uniform_color(const std::vector<std::uint8_t>& pixels, gcanvas::color expected)
    {
        if (pixels.size() < 4 || pixels.size() % 4 != 0)
        {
            return false;
        }
        const std::size_t center = (pixels.size() / 8U) * 4U;
        return near_channel(pixels[center], expected.r()) &&
               near_channel(pixels[center + 1U], expected.g()) &&
               near_channel(pixels[center + 2U], expected.b()) &&
               near_channel(pixels[center + 3U], expected.a());
    }

    bool render_and_verify(gcanvas::Context& canvas, int width, int height, gcanvas::color color)
    {
        canvas.set_clear_color(color);
        canvas.set_fill_color(color);
        canvas.fill_rect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        canvas.draw_frame();
        const auto pixels = canvas.read_pixels();
        const bool verified = verify_uniform_color(pixels, color);
        canvas.present_frame();
        return verified;
    }

    bool render_image_and_verify(gcanvas::Context& canvas, gcanvas::Image& image, int width,
                                 int height, gcanvas::color expected)
    {
        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.draw_image(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), image);
        canvas.draw_frame();
        const auto pixels = canvas.read_pixels();
        const bool verified = verify_uniform_color(pixels, expected);
        canvas.present_frame();
        return verified;
    }

    std::array<std::uint8_t, 4> sample_pixel(const std::vector<std::uint8_t>& pixels,
                                             const std::string& backend, int width, int height,
                                             int x, int y)
    {
        const int storage_y = backend == "opengl" ? height - 1 - y : y;
        const std::size_t offset =
            (static_cast<std::size_t>(storage_y) * width + x) * 4U;
        return {pixels[offset], pixels[offset + 1U], pixels[offset + 2U], pixels[offset + 3U]};
    }

    std::array<std::uint8_t, 4> sample_logical_pixel(
        const gcanvas::Context& canvas, const std::vector<std::uint8_t>& pixels,
        const std::string& backend, int width, int height, float x, float y)
    {
        const gcanvas::CanvasMetrics& metrics = canvas.metrics();
        const int physical_x = static_cast<int>(std::lround(
            (x + metrics.offset_x) * metrics.scale_x * metrics.dpi_scale));
        const int physical_y = static_cast<int>(std::lround(
            (y + metrics.offset_y) * metrics.scale_y * metrics.dpi_scale));
        return sample_pixel(pixels, backend, width, height, physical_x, physical_y);
    }

    bool render_affine_image_and_verify(gcanvas::Context& canvas, gcanvas::Image& image,
                                        const std::string& backend, int width, int height)
    {
        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        const gcanvas::Transform transform =
            gcanvas::Transform::translation(50.0f, 10.0f) *
            gcanvas::Transform::rotation(3.14159265358979323846f * 0.5f);
        canvas.draw_image(0.0f, 0.0f, 20.0f, 8.0f, image, transform);
        canvas.draw_frame();
        const auto pixels = canvas.read_pixels();
        const auto inside =
            sample_logical_pixel(canvas, pixels, backend, width, height, 46.0f, 20.0f);
        const auto outside =
            sample_logical_pixel(canvas, pixels, backend, width, height, 20.0f, 20.0f);
        canvas.present_frame();
        const bool verified = inside[0] >= 40 && inside[0] <= 70 && inside[1] > 130 &&
                              inside[2] > 200 &&
                              outside[0] < 20 && outside[1] < 20 && outside[2] < 20;
        if (!verified)
        {
            std::cerr << backend << " affine image samples: inside=("
                      << static_cast<int>(inside[0]) << ',' << static_cast<int>(inside[1]) << ','
                      << static_cast<int>(inside[2]) << ',' << static_cast<int>(inside[3])
                      << ") outside=(" << static_cast<int>(outside[0]) << ','
                      << static_cast<int>(outside[1]) << ',' << static_cast<int>(outside[2]) << ','
                      << static_cast<int>(outside[3]) << ")\n";
        }
        return verified;
    }

    std::size_t count_bright_pixels(const gcanvas::Context& canvas,
                                    const std::vector<std::uint8_t>& pixels,
                                    const std::string& backend, int width, int height,
                                    int minimum_x, int minimum_y, int maximum_x, int maximum_y)
    {
        std::size_t count = 0;
        for (int y = minimum_y; y < maximum_y; ++y)
        {
            for (int x = minimum_x; x < maximum_x; ++x)
            {
                const auto pixel = sample_logical_pixel(
                    canvas, pixels, backend, width, height, static_cast<float>(x),
                    static_cast<float>(y));
                if (pixel[0] > 180 && pixel[1] > 180 && pixel[2] > 180)
                    ++count;
            }
        }
        return count;
    }

    bool render_affine_text_and_verify(gcanvas::Context& canvas, const std::string& backend,
                                       int width, int height)
    {
        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.set_fill_color(gcanvas::color(255, 255, 255, 255));
        canvas.set_font_size(24);
        const gcanvas::Transform transform =
            gcanvas::Transform::translation(75.0f, 5.0f) *
            gcanvas::Transform::rotation(3.14159265358979323846f * 0.5f);
        canvas.draw_text(0.0f, 0.0f, "HI", transform);
        canvas.draw_frame();
        const auto pixels = canvas.read_pixels();
        const std::size_t rotated =
            count_bright_pixels(canvas, pixels, backend, width, height, 48, 6, 73, 30);
        const std::size_t untransformed =
            count_bright_pixels(canvas, pixels, backend, width, height, 80, 6, 95, 32);
        canvas.present_frame();
        if (rotated < 8U || untransformed != 0U)
        {
            std::cerr << backend << " affine text coverage mismatch: rotated=" << rotated
                      << " untransformed=" << untransformed << '\n';
            return false;
        }
        return true;
    }

    bool verify_alpha_features(gcanvas::Context& canvas, gcanvas::Image& image,
                               const std::string& backend, int width, int height)
    {
        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.set_fill_color(gcanvas::color(255, 255, 255, 128));
        canvas.draw_image(4.0f, 4.0f, 24.0f, 16.0f, image, true);
        canvas.draw_frame();
        auto pixels = canvas.read_pixels();
        const auto tinted_image = sample_logical_pixel(
            canvas, pixels, backend, width, height, 12.0f, 12.0f);
        canvas.present_frame();
        const bool image_ok = tinted_image[0] >= 20 && tinted_image[0] <= 34 &&
                              tinted_image[1] >= 68 && tinted_image[1] <= 86 &&
                              tinted_image[2] >= 100 && tinted_image[2] <= 120;
        if (!image_ok)
        {
            std::cerr << backend << " tinted image alpha mismatch\n";
            return false;
        }

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.set_stroke_color(gcanvas::color(255, 0, 0, 128));
        canvas.set_line_width(4.0f);
        canvas.stroke_rect(36.0f, 4.0f, 24.0f, 16.0f);
        canvas.draw_frame();
        pixels = canvas.read_pixels();
        const auto stroke = sample_logical_pixel(
            canvas, pixels, backend, width, height, 38.0f, 12.0f);
        canvas.present_frame();
        const bool stroke_ok = stroke[0] >= 110 && stroke[0] <= 145 &&
                               stroke[1] < 16 && stroke[2] < 16;
        if (!stroke_ok)
        {
            std::cerr << backend << " stroke alpha mismatch: sample=("
                      << static_cast<int>(stroke[0]) << ',' << static_cast<int>(stroke[1]) << ','
                      << static_cast<int>(stroke[2]) << ',' << static_cast<int>(stroke[3])
                      << ")\n";
            return false;
        }

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.set_fill_color(gcanvas::color(255, 255, 255, 128));
        canvas.use_default_font();
        canvas.set_font_size(24);
        canvas.draw_text(64.0f, 0.0f, "A");
        canvas.draw_frame();
        pixels = canvas.read_pixels();
        std::uint8_t brightest_text = 0;
        for (int y = 0; y < 32; ++y)
        {
            for (int x = 62; x < width; ++x)
            {
                const auto pixel = sample_pixel(pixels, backend, width, height, x, y);
                brightest_text = (std::max)(brightest_text, pixel[0]);
            }
        }
        canvas.present_frame();
        const bool text_ok = brightest_text >= 110 && brightest_text <= 145;
        if (!text_ok)
        {
            std::cerr << backend << " text alpha mismatch: brightest="
                      << static_cast<int>(brightest_text) << '\n';
        }
        return text_ok;
    }

    bool verify_shadow_features(gcanvas::Context& canvas, const std::string& backend,
                                int width, int height)
    {
        bool rejected_negative_blur = false;
        try
        {
            canvas.draw_rect_shadow(24.0f, 14.0f, 24.0f, 16.0f, -1.0f);
        }
        catch (const std::invalid_argument&)
        {
            rejected_negative_blur = true;
        }
        if (!rejected_negative_blur)
        {
            std::cerr << backend << " accepted a negative shadow blur radius\n";
            return false;
        }

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.set_fill_color(gcanvas::color(255, 255, 255, 255));
        canvas.draw_rect_shadow(24.0f, 14.0f, 24.0f, 16.0f, 6.0f);
        canvas.draw_frame();
        auto pixels = canvas.read_pixels();
        const auto interior = sample_logical_pixel(
            canvas, pixels, backend, width, height, 30.0f, 22.0f);
        const auto falloff = sample_logical_pixel(
            canvas, pixels, backend, width, height, 21.0f, 22.0f);
        const auto exterior = sample_logical_pixel(
            canvas, pixels, backend, width, height, 16.0f, 22.0f);
        canvas.present_frame();
        const bool rect_ok = interior[0] > 240 && falloff[0] > 45 && falloff[0] < 210 &&
                             exterior[0] < 16;
        if (!rect_ok)
        {
            std::cerr << backend << " rect shadow samples: interior="
                      << static_cast<int>(interior[0]) << " falloff="
                      << static_cast<int>(falloff[0]) << " exterior="
                      << static_cast<int>(exterior[0]) << '\n';
            return false;
        }

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas.draw_rounded_rect_shadow(58.0f, 12.0f, 20.0f, 20.0f, 6.0f, 4.0f);
        canvas.draw_circle_shadow(40.0f, 24.0f, 7.0f, 4.0f);
        canvas.draw_frame();
        pixels = canvas.read_pixels();
        const auto rounded_center = sample_logical_pixel(
            canvas, pixels, backend, width, height, 68.0f, 22.0f);
        const auto circle_center = sample_logical_pixel(
            canvas, pixels, backend, width, height, 40.0f, 24.0f);
        const auto circle_falloff = sample_logical_pixel(
            canvas, pixels, backend, width, height, 49.0f, 24.0f);
        canvas.present_frame();
        const bool primitive_ok = rounded_center[0] > 240 && circle_center[0] > 240 &&
                                  circle_falloff[0] > 20 && circle_falloff[0] < 235;
        if (!primitive_ok)
        {
            std::cerr << backend << " rounded/circle shadow coverage mismatch\n";
        }
        return primitive_ok;
    }

    bool verify_path_features(gcanvas::Context& canvas, const std::string& backend,
                              int width, int height)
    {
        const auto sample_logical = [&](const std::vector<std::uint8_t>& pixels, float x, float y) {
            return sample_logical_pixel(canvas, pixels, backend, width, height, x, y);
        };
        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        gcanvas::Path gradient_rect;
        gradient_rect.rect(8.0f, 8.0f, 80.0f, 24.0f);
        const gcanvas::Paint gradient = gcanvas::Paint::linear_gradient(
            8.0f, 8.0f, 88.0f, 8.0f,
            {{0.0f, gcanvas::color(255, 0, 0, 255)},
             {0.5f, gcanvas::color(0, 255, 0, 255)},
             {1.0f, gcanvas::color(0, 0, 255, 255)}});
        canvas.fill_path(gradient_rect, gradient);
        canvas.draw_frame();
        auto pixels = canvas.read_pixels();
        const auto left = sample_logical(pixels, 16.0f, 20.0f);
        const auto right = sample_logical(pixels, 80.0f, 20.0f);
        const bool gradient_ok = left[0] > left[2] && right[2] > right[0] &&
                                 left[3] > 240 && right[3] > 240;
        canvas.present_frame();
        if (!gradient_ok)
        {
            int min_x = width;
            int min_y = height;
            int max_x = -1;
            int max_y = -1;
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const auto pixel = sample_pixel(pixels, backend, width, height, x, y);
                    if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0)
                    {
                        min_x = std::min(min_x, x);
                        min_y = std::min(min_y, y);
                        max_x = std::max(max_x, x);
                        max_y = std::max(max_y, y);
                    }
                }
            }
            std::cerr << backend << " gradient samples: left=(" << static_cast<int>(left[0])
                      << ',' << static_cast<int>(left[1]) << ',' << static_cast<int>(left[2])
                      << ',' << static_cast<int>(left[3]) << ") right=("
                      << static_cast<int>(right[0]) << ',' << static_cast<int>(right[1]) << ','
                      << static_cast<int>(right[2]) << ',' << static_cast<int>(right[3])
                      << ") bounds=(" << min_x << ',' << min_y << ")-(" << max_x << ','
                      << max_y << ")\n";
            return false;
        }

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        gcanvas::Path rotated_rect;
        rotated_rect.rect(0.0f, 0.0f, 20.0f, 8.0f);
        const gcanvas::Transform transform =
            gcanvas::Transform::translation(50.0f, 10.0f) *
            gcanvas::Transform::rotation(3.14159265358979323846f * 0.5f);
        canvas.fill_path(rotated_rect, gcanvas::Paint::solid(gcanvas::color(0, 255, 0, 255)),
                         transform);
        canvas.draw_frame();
        pixels = canvas.read_pixels();
        const auto inside = sample_logical(pixels, 46.0f, 20.0f);
        const auto outside = sample_logical(pixels, 20.0f, 20.0f);
        const bool transform_ok = inside[1] > 220 && inside[0] < 20 &&
                                  outside[0] < 20 && outside[1] < 20 && outside[2] < 20;
        canvas.present_frame();
        if (!transform_ok)
        {
            int min_x = width;
            int min_y = height;
            int max_x = -1;
            int max_y = -1;
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const auto pixel = sample_pixel(pixels, backend, width, height, x, y);
                    if (pixel[1] > 32)
                    {
                        min_x = std::min(min_x, x);
                        min_y = std::min(min_y, y);
                        max_x = std::max(max_x, x);
                        max_y = std::max(max_y, y);
                    }
                }
            }
            std::cerr << backend << " transform samples: inside=("
                      << static_cast<int>(inside[0]) << ',' << static_cast<int>(inside[1]) << ','
                      << static_cast<int>(inside[2]) << ',' << static_cast<int>(inside[3])
                      << ") outside=(" << static_cast<int>(outside[0]) << ','
                      << static_cast<int>(outside[1]) << ',' << static_cast<int>(outside[2]) << ','
                      << static_cast<int>(outside[3]) << ") bounds=(" << min_x << ',' << min_y
                      << ")-(" << max_x << ',' << max_y << ")\n";
        }
        if (!transform_ok)
            return false;

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        gcanvas::Path even_odd;
        even_odd.rect(4.0f, 4.0f, 28.0f, 28.0f);
        even_odd.rect(11.0f, 11.0f, 14.0f, 14.0f);
        canvas.fill_path(even_odd,
                         gcanvas::Paint::solid(gcanvas::color(255, 0, 255, 255)));
        canvas.draw_frame();
        pixels = canvas.read_pixels();
        const auto ring = sample_logical(pixels, 7.0f, 18.0f);
        const auto hole = sample_logical(pixels, 18.0f, 18.0f);
        const bool even_odd_ok = ring[0] > 220 && ring[2] > 220 && hole[0] < 20 &&
                                 hole[1] < 20 && hole[2] < 20;
        canvas.present_frame();
        if (!even_odd_ok)
        {
            std::cerr << backend << " even-odd path pixel mismatch\n";
            return false;
        }

        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        gcanvas::Path ellipse;
        ellipse.ellipse(20.0f, 18.0f, 8.0f, 5.0f);
        canvas.fill_path(
            ellipse,
            gcanvas::Paint::radial_gradient(
                20.0f, 18.0f, 0.0f, 8.0f,
                {{0.0f, gcanvas::color(255, 0, 0, 255)},
                 {1.0f, gcanvas::color(0, 0, 255, 255)}}));
        gcanvas::Path line;
        line.move_to(35.0f, 10.0f).line_to(55.0f, 30.0f);
        canvas.stroke_path(line, gcanvas::Paint::solid(gcanvas::color(255, 255, 255, 255)),
                           3.0f);
        gcanvas::Path cubic;
        cubic.move_to(60.0f, 30.0f).cubic_to(65.0f, 5.0f, 80.0f, 5.0f, 85.0f, 30.0f);
        canvas.stroke_path(cubic, gcanvas::Paint::solid(gcanvas::color(255, 255, 0, 255)),
                           2.0f);
        canvas.draw_frame();
        pixels = canvas.read_pixels();
        const auto ellipse_center = sample_logical(pixels, 20.0f, 18.0f);
        const auto ellipse_edge = sample_logical(pixels, 26.0f, 18.0f);
        const auto line_center = sample_logical(pixels, 45.0f, 20.0f);
        const auto cubic_center = sample_logical(pixels, 72.5f, 11.25f);
        const bool shapes_ok = ellipse_center[0] > ellipse_center[2] &&
                               ellipse_edge[2] > ellipse_edge[0] &&
                               line_center[0] > 220 && line_center[1] > 220 &&
                               line_center[2] > 220 && cubic_center[0] > 220 &&
                               cubic_center[1] > 220 && cubic_center[2] < 32;
        canvas.present_frame();
        if (!shapes_ok)
        {
            std::cerr << backend << " ellipse/line/cubic pixel mismatch\n";
        }
        return shapes_ok;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: gcanvas_gpu_test <opengl|vulkan>\n";
        return 2;
    }

    try
    {
        constexpr int initial_width = 64;
        constexpr int initial_height = 64;
        gcanvas::WindowConfig config{"gCanvas GPU test", initial_width, initial_height};
        config.visible = false;
        config.vsync = false;
        const std::string backend = argv[1];
        if (backend == "opengl")
        {
            config.backend = gcanvas::Backend::OpenGL;
        }
        else if (backend == "vulkan")
        {
            config.backend = gcanvas::Backend::Vulkan;
        }
        else
        {
            std::cerr << "unknown backend: " << backend << '\n';
            return 2;
        }

        auto window = gcanvas::Window::create(config);
        gcanvas::Context& canvas = window->create_context();
        if (!render_and_verify(canvas, canvas.get_width(), canvas.get_height(),
                               gcanvas::color(231, 76, 60, 255)))
        {
            std::cerr << backend << " initial pixel readback mismatch\n";
            return 3;
        }

        // Frame submission is a single-use protocol. Repeated presentation and an empty frame
        // must remain no-ops instead of reusing an acquired image or consumed semaphore.
        canvas.present_frame();
        canvas.draw_frame();
        canvas.present_frame();
        if (!render_and_verify(canvas, canvas.get_width(), canvas.get_height(),
                               gcanvas::color(155, 89, 182, 255)))
        {
            std::cerr << backend << " did not recover from idle frame calls\n";
            return 10;
        }

        if (backend == "vulkan")
        {
            const std::array<gcanvas::color, 5> queued_colors = {
                gcanvas::color(26, 188, 156, 255), gcanvas::color(52, 152, 219, 255),
                gcanvas::color(241, 196, 15, 255), gcanvas::color(230, 126, 34, 255),
                gcanvas::color(149, 165, 166, 255)};
            for (const gcanvas::color& frame_color : queued_colors)
            {
                canvas.set_clear_color(frame_color);
                canvas.set_fill_color(frame_color);
                canvas.fill_rect(0.0f, 0.0f, static_cast<float>(canvas.get_width()),
                                 static_cast<float>(canvas.get_height()));
                canvas.draw_frame();
                canvas.present_frame();
            }
            if (!render_and_verify(canvas, canvas.get_width(), canvas.get_height(),
                                   gcanvas::color(142, 68, 173, 255)))
            {
                std::cerr << "vulkan frame-ring reuse corrupted the next frame\n";
                return 17;
            }
        }

        constexpr int resized_width = 96;
        constexpr int resized_height = 48;
        window->set_size(resized_width, resized_height);
        window->poll_events();
        const int actual_width = canvas.get_width();
        const int actual_height = canvas.get_height();
        if (!render_and_verify(canvas, actual_width, actual_height,
                               gcanvas::color(46, 204, 113, 255)))
        {
            std::cerr << backend << " resized pixel readback mismatch\n";
            return 4;
        }

        if (!verify_path_features(canvas, backend, actual_width, actual_height))
        {
            std::cerr << backend << " path/gradient/transform pixel mismatch\n";
            return 5;
        }

        if (!render_affine_text_and_verify(canvas, backend, actual_width, actual_height))
            return 15;

        std::array<std::uint8_t, 16> image_pixels = {
            231, 76, 60, 255, 52, 152, 219, 255,
            231, 76, 60, 255, 52, 152, 219, 255};
        bool rejected_short_span = false;
        try
        {
            canvas.create_image(2, 2, 4, image_pixels.data(), image_pixels.size() - 1U);
        }
        catch (const std::invalid_argument&)
        {
            rejected_short_span = true;
        }
        if (!rejected_short_span)
        {
            std::cerr << backend << " accepted a short image span\n";
            return 6;
        }

        gcanvas::Image& image =
            canvas.create_image(2, 2, 4, image_pixels.data(), image_pixels.size());
        canvas.set_clear_color(gcanvas::color(0, 0, 0, 255));
        gcanvas::Path pattern_rect;
        pattern_rect.rect(8.0f, 8.0f, 24.0f, 16.0f);
        canvas.fill_path(pattern_rect,
                         gcanvas::Paint::image_pattern(image, 8.0f, 8.0f, 8.0f, 16.0f));
        canvas.draw_frame();
        const auto pattern_pixels = canvas.read_pixels();
        const auto pattern_left = sample_logical_pixel(
            canvas, pattern_pixels, backend, actual_width, actual_height, 10.0f, 16.0f);
        const auto pattern_right = sample_logical_pixel(
            canvas, pattern_pixels, backend, actual_width, actual_height, 14.0f, 16.0f);
        const auto pattern_repeated = sample_logical_pixel(
            canvas, pattern_pixels, backend, actual_width, actual_height, 18.0f, 16.0f);
        const auto pattern_outside = sample_logical_pixel(
            canvas, pattern_pixels, backend, actual_width, actual_height, 48.0f, 16.0f);
        const bool pattern_ok = pattern_left[0] > pattern_left[2] &&
                                pattern_right[2] > pattern_right[0] && pattern_outside[0] < 20 &&
                                pattern_outside[1] < 20 && pattern_outside[2] < 20 &&
                                pattern_repeated[0] > pattern_repeated[2];
        canvas.present_frame();
        if (!pattern_ok)
        {
            std::cerr << backend << " image pattern pixel mismatch\n";
            return 7;
        }
        image_pixels = {52, 152, 219, 255, 52, 152, 219, 255,
                        52, 152, 219, 255, 52, 152, 219, 255};
        image.update_data(image_pixels.data(), image_pixels.size());
        if (!render_image_and_verify(canvas, image, actual_width, actual_height,
                                     gcanvas::color(52, 152, 219, 255)))
        {
            std::cerr << backend << " dynamic image pixel mismatch\n";
            return 8;
        }

        if (!render_affine_image_and_verify(canvas, image, backend, actual_width,
                                            actual_height))
        {
            std::cerr << backend << " affine image pixel mismatch\n";
            return 14;
        }

        if (!verify_alpha_features(canvas, image, backend, actual_width, actual_height))
        {
            return 13;
        }

        if (!verify_shadow_features(canvas, backend, actual_width, actual_height))
        {
            return 16;
        }

        image_pixels = {241, 196, 15, 255, 241, 196, 15, 255,
                        241, 196, 15, 255, 241, 196, 15, 255};
        image.update_data(image_pixels.data(), image_pixels.size());
        if (!render_image_and_verify(canvas, image, actual_width, actual_height,
                                     gcanvas::color(241, 196, 15, 255)))
        {
            std::cerr << backend << " updated image pixel mismatch\n";
            return 9;
        }

        if (backend == "vulkan")
        {
            constexpr std::size_t vulkan_draw_capacity = 65536U * 2U;
            bool rejected_overflow = false;
            try
            {
                for (std::size_t index = 0; index <= vulkan_draw_capacity; ++index)
                    canvas.fill_rect(0.0f, 0.0f, 1.0f, 1.0f);
            }
            catch (const std::length_error&)
            {
                rejected_overflow = true;
            }
            if (!rejected_overflow)
            {
                std::cerr << "vulkan accepted draw records beyond its GPU buffer capacity\n";
                return 11;
            }
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "GPU test failed: " << error.what() << '\n';
        return 12;
    }
}
