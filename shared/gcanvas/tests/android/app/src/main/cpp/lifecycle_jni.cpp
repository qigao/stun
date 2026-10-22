#include <jni.h>
#include <android/native_window_jni.h>

#include "gcanvas/backends/opengles.hpp"
#include "gcanvas/platform/android_egl.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    std::unique_ptr<gcanvas::android::AndroidEglHost> host;
    std::unique_ptr<gcanvas::Context> context;
    gcanvas::Image* persistent_image = nullptr;
    int stage = 0;
    std::string failure;

    std::string center_description(const std::vector<std::uint8_t>& pixels,
                                   int width, int height)
    {
        const std::size_t expected =
            width > 0 && height > 0
                ? static_cast<std::size_t>(width) *
                      static_cast<std::size_t>(height) * 4U
                : 0U;
        if (pixels.size() != expected || expected == 0U)
        {
            return "framebuffer=" + std::to_string(width) + "x" +
                   std::to_string(height) + " bytes=" +
                   std::to_string(pixels.size());
        }

        const std::size_t offset =
            (static_cast<std::size_t>(height / 2) *
                 static_cast<std::size_t>(width) +
             static_cast<std::size_t>(width / 2)) *
            4U;
        return "framebuffer=" + std::to_string(width) + "x" +
               std::to_string(height) + " center=(" +
               std::to_string(pixels[offset]) + "," +
               std::to_string(pixels[offset + 1U]) + "," +
               std::to_string(pixels[offset + 2U]) + "," +
               std::to_string(pixels[offset + 3U]) + ")";
    }

    bool center_matches(const std::vector<std::uint8_t>& pixels,
                        int width, int height,
                        std::uint8_t red, std::uint8_t green,
                        std::uint8_t blue)
    {
        if (width <= 0 || height <= 0 ||
            pixels.size() != static_cast<std::size_t>(width) *
                                 static_cast<std::size_t>(height) * 4U)
            return false;

        const std::size_t offset =
            (static_cast<std::size_t>(height / 2) *
                 static_cast<std::size_t>(width) +
             static_cast<std::size_t>(width / 2)) *
            4U;

        constexpr int tolerance = 32;
        const auto close = [](std::uint8_t actual, std::uint8_t expected) {
            const int delta = static_cast<int>(actual) - static_cast<int>(expected);
            return delta >= -tolerance && delta <= tolerance;
        };
        return close(pixels[offset], red) &&
               close(pixels[offset + 1U], green) &&
               close(pixels[offset + 2U], blue) &&
               pixels[offset + 3U] >= 220U;
    }

    void render_first_frame()
    {
        const int width = host->framebuffer_width();
        const int height = host->framebuffer_height();

        context->set_clear_color(gcanvas::color(0, 0, 0, 255));
        context->set_fill_color(gcanvas::color(255, 0, 0, 255));
        context->fill_rect(0.0f, 0.0f,
                           static_cast<float>(width),
                           static_cast<float>(height));
        context->draw_image(0.0f, 0.0f, 16.0f, 16.0f, *persistent_image);
        context->draw_frame();

        const auto pixels = context->read_pixels();
        if (!center_matches(pixels, width, height, 255U, 0U, 0U))
            throw std::runtime_error(
                "first GLES frame readback is not red: " +
                center_description(pixels, width, height));

        context->present_frame();
    }

    void render_second_frame()
    {
        const int width = host->framebuffer_width();
        const int height = host->framebuffer_height();

        context->set_clear_color(gcanvas::color(0, 0, 0, 255));
        context->draw_image(0.0f, 0.0f,
                            static_cast<float>(width),
                            static_cast<float>(height),
                            *persistent_image);

        context->set_fill_color(gcanvas::color(255, 255, 255, 255));
        context->draw_text(8.0f, 28.0f, std::string("GLES"));
        context->fill_rounded_rect(8.0f, 36.0f, 48.0f, 24.0f, 6.0f);
        context->fill_circle(static_cast<float>(width - 24), 24.0f, 12.0f);

        context->draw_frame();

        const auto pixels = context->read_pixels();
        if (!center_matches(pixels, width, height, 0U, 255U, 0U))
            throw std::runtime_error(
                "persistent texture did not survive Android Surface replacement: " +
                center_description(pixels, width, height));

        context->present_frame();
    }

    void destroy_state() noexcept
    {
        persistent_image = nullptr;
        context.reset();
        host.reset();
        stage = 0;
    }

    jint on_surface_created(JNIEnv* env, jobject surface)
    {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window == nullptr)
            throw std::runtime_error("ANativeWindow_fromSurface returned null");

        struct WindowRelease
        {
            ANativeWindow* value;
            ~WindowRelease()
            {
                if (value != nullptr)
                    ANativeWindow_release(value);
            }
        } release{window};

        if (!host)
        {
            host = gcanvas::android::AndroidEglHost::create(window);

            gcanvas::opengles::CreateInfo create_info{};
            create_info.metrics.width = host->framebuffer_width();
            create_info.metrics.height = host->framebuffer_height();
            create_info.host = host->host_callbacks();

            context = gcanvas::opengles::create_context(create_info);
            if (!context || context->backend() != gcanvas::Backend::OpenGLES)
                throw std::runtime_error("OpenGLES context creation failed");

            const std::uint8_t green_pixel[4] = {0U, 255U, 0U, 255U};
            persistent_image = &context->create_image(
                1, 1, 4, green_pixel, sizeof(green_pixel));

            render_first_frame();
            stage = 1;
            return stage;
        }

        if (stage != 1 || !context || persistent_image == nullptr)
            throw std::logic_error("unexpected Android lifecycle stage before resume");

        host->resume(*context, window);
        render_second_frame();
        stage = 2;
        return stage;
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_qigao_gcanvas_lifecycle_MainActivity_nativeSurfaceCreated(
    JNIEnv* env, jclass, jobject surface)
{
    try
    {
        if (!failure.empty())
            return -1;
        return on_surface_created(env, surface);
    }
    catch (const std::exception& error)
    {
        failure = error.what();
        return -1;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_qigao_gcanvas_lifecycle_MainActivity_nativeSurfaceDestroyed(
    JNIEnv*, jclass)
{
    try
    {
        if (host && context && host->has_surface())
            host->suspend(*context);
    }
    catch (const std::exception& error)
    {
        failure = error.what();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_qigao_gcanvas_lifecycle_MainActivity_nativeDestroy(
    JNIEnv*, jclass)
{
    destroy_state();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_qigao_gcanvas_lifecycle_MainActivity_nativeFailure(
    JNIEnv* env, jclass)
{
    return env->NewStringUTF(failure.empty() ? "unknown native error" : failure.c_str());
}
