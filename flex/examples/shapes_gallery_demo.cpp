/*
 * Shapes Gallery Demo
 * Demonstrates all shape types in Flex DSL:
 * rect, circle, ellipse, polygon, star, line, ring, path
 * Also shows: stroke, fill, rough (hand-drawn) style
 */

#include <iostream>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

class ShapesGalleryDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Shapes Gallery - Flex DSL",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH, HEIGHT,
            SDL_WINDOW_SHOWN
        );
        if (!window_) return false;

        sdl_renderer_ = SDL_CreateRenderer(window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdl_renderer_) return false;

        texture_ = SDL_CreateTexture(sdl_renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            WIDTH, HEIGHT);
        if (!texture_) return false;

        if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

        canvas_ = tvg::SwCanvas::gen();
        if (!canvas_) return false;

        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        flex::init();

        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        std::cout << "Loading Shapes Gallery...\n";
        auto definition = flex::Definition::load_file("shapes_gallery.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            std::cerr << "  Line: " << definition->error_line() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) {
            std::cerr << "No scene created!\n";
            return false;
        }

        instance_->scene()->set_size(WIDTH, HEIGHT);

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        std::cout << "Shapes Gallery initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  ESC - Quit\n";
        std::cout << "  S   - Save to PNG\n\n";

        return true;
    }

    void run() {
        running_ = true;
        Uint32 last_time = SDL_GetTicks();

        while (running_) {
            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;

            handle_events();
            update(dt);
            render();
            SDL_Delay(16);
        }
    }

    ~ShapesGalleryDemo() {
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();

        if (canvas_) delete canvas_;
        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    static constexpr int WIDTH = 900;
    static constexpr int HEIGHT = 700;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    bool running_ = false;
    float time_ = 0.0f;

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running_ = false;
                    } else if (event.key.keysym.sym == SDLK_s) {
                        save_png();
                    }
                    break;
            }
        }
    }

    void save_png() {
        // Convert ARGB to RGBA for PNG
        std::vector<uint8_t> rgba(WIDTH * HEIGHT * 4);
        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            uint32_t c = buffer_[i];
            rgba[i*4+0] = (c >> 16) & 0xFF;  // R
            rgba[i*4+1] = (c >> 8) & 0xFF;   // G
            rgba[i*4+2] = c & 0xFF;          // B
            rgba[i*4+3] = (c >> 24) & 0xFF;  // A
        }
        stbi_write_png("shapes_gallery.png", WIDTH, HEIGHT, 4, rgba.data(), WIDTH * 4);
        std::cout << "Saved: shapes_gallery.png\n";
    }

    void update(float dt) {
        time_ += dt;
        instance_->advance(dt);
    }

    void render() {
        canvas_->remove();

        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
        flex_renderer_->clear(instance_->scene()->background());
        instance_->render(*flex_renderer_);
        flex_renderer_->end_frame();

        canvas_->draw();
        canvas_->sync();

        SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));
        SDL_RenderClear(sdl_renderer_);
        SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer_);
    }
};

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "Shapes Gallery - Flex DSL Demo\n";
    std::cout << "===========================================\n\n";

    ShapesGalleryDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
