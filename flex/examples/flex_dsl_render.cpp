/*
 * Flex Engine - DSL Rendering Demo
 *
 * Loads a .flex file and renders it using ThorVG + SDL2.
 * This demonstrates the complete DSL -> Render pipeline.
 */

#include <iostream>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

class DslRenderDemo {
public:
    bool init(const char* flex_file) {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Flex DSL Render Demo",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH, HEIGHT,
            SDL_WINDOW_SHOWN
        );
        if (!window_) {
            std::cerr << "Window creation failed\n";
            return false;
        }

        // Create SDL renderer
        sdl_renderer_ = SDL_CreateRenderer(window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdl_renderer_) {
            std::cerr << "SDL renderer creation failed\n";
            return false;
        }

        // Create texture for ThorVG output
        texture_ = SDL_CreateTexture(sdl_renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            WIDTH, HEIGHT);
        if (!texture_) {
            std::cerr << "Texture creation failed\n";
            return false;
        }

        // Initialize ThorVG
        if (tvg::Initializer::init(0) != tvg::Result::Success) {
            std::cerr << "ThorVG init failed\n";
            return false;
        }

        // Create ThorVG software canvas (raw pointer)
        canvas_ = tvg::SwCanvas::gen();
        if (!canvas_) {
            std::cerr << "ThorVG canvas creation failed\n";
            return false;
        }

        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        // Initialize Flex
        flex::init();

          if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        // Load .flex file
        std::cout << "Loading: " << flex_file << "\n";
        auto definition = flex::Definition::load_file(flex_file);

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) {
            std::cerr << "No scene in definition\n";
            return false;
        }

        // Create ThorVG renderer
        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        std::cout << "Initialized successfully!\n";
        std::cout << "Press ESC to quit, SPACE to send click event\n\n";
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
            SDL_Delay(16);  // Limit to ~60 FPS
        }
    }

    ~DslRenderDemo() {
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();

        if (canvas_) {
            delete canvas_;
            canvas_ = nullptr;
        }
        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;  // ThorVG uses raw pointers
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    bool running_ = false;
    float time_ = 0;

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
                    } else if (event.key.keysym.sym == SDLK_SPACE) {
                        instance_->send_event("click");
                        std::cout << "Sent 'click' event\n";
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    instance_->send_pointer_event(
                        static_cast<float>(event.button.x),
                        static_cast<float>(event.button.y),
                        true
                    );
                    break;
            }
        }
    }

    void update(float dt) {
        time_ += dt;
        instance_->advance(dt);

        // Simple animation: move player based on time
        auto* player = instance_->scene()->find("player");
        if (player) {
            float wave = std::sin(time_ * 2.0f) * 20.0f;
            player->set_y(300 + wave);
        }
    }

    void render() {
        // Clear ThorVG canvas (remove all paints from previous frame)
        canvas_->remove();

        // Render flex scene to ThorVG
        auto* scene = instance_->scene();
        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);

        // Clear background
        flex_renderer_->clear(scene->background());

        // Render scene
        instance_->render(*flex_renderer_);

        flex_renderer_->end_frame();

        // Draw and sync ThorVG canvas (true = clear buffer first)
        canvas_->draw(true);
        canvas_->sync();

        // Copy buffer to SDL texture
        SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));

        // Render to screen
        SDL_RenderClear(sdl_renderer_);
        SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer_);
    }
};

int main(int argc, char* argv[]) {
    const char* flex_file = "hello.flex";
    if (argc > 1) {
        flex_file = argv[1];
    }

    DslRenderDemo demo;
    if (!demo.init(flex_file)) {
        return 1;
    }

    demo.run();
    return 0;
}
