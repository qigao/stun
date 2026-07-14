/*
 * Loading Animation Demo
 * Multiple animated spinners and progress indicators
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "backends/thorvg/init.h"

class LoadingAnimationDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Loading Animations - Flex Engine Demo",
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

        // Create ThorVG software canvas
        canvas_ = tvg::SwCanvas::gen();
        if (!canvas_) {
            std::cerr << "ThorVG canvas creation failed\n";
            return false;
        }

        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        // Initialize Flex
        flex::init();

        // Load font
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        // Load loading animations
        std::cout << "Loading Animations Demo...\n";
        auto definition = flex::Definition::load_file("loading_animation.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message()
                      << " at line " << definition->error_line()
                      << ", column " << definition->error_column() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) {
            std::cerr << "No scene in definition\n";
            return false;
        }

        // Create ThorVG renderer
        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Get scene for manual nodes
        auto* scene = instance_->scene();

        // Progress bar nodes (we need to update text manually)
        progress_fill_ = scene->find("progress");
        progress_text_ = scene->find("percentage");

        // Start all declarative animations from .flex file
        auto check_anim = [&](const char* name) {
            auto* player = instance_->play_animation(name);
            if (player) {
                auto* tl = player->timeline();
                const char* loop_str = "once";
                if (tl->loop_mode() == flex::LoopMode::Loop) loop_str = "loop";
                else if (tl->loop_mode() == flex::LoopMode::PingPong) loop_str = "pingpong";
                std::cout << "  [OK] '" << name << "' (loop=" << loop_str << ", dur=" << tl->duration() << "s)\n";
            } else {
                std::cout << "  [FAIL] Animation '" << name << "' NOT FOUND\n";
            }
        };

        std::cout << "\nStarting animations:\n";
        check_anim("spin");         // Spinner 1: rotation
        check_anim("pulse");        // Spinner 2: pulsing opacity
        check_anim("bounce1");      // Spinner 3: bouncing dots
        check_anim("bounce2");
        check_anim("bounce3");
        check_anim("bounce4");
        check_anim("progressFill"); // Progress bar width
        check_anim("squareSpin");   // Spinner 4: square rotation
        check_anim("fadeWave1");    // Spinner 5: fading circles
        check_anim("fadeWave2");
        check_anim("fadeWave3");
        check_anim("scalePulse");   // Spinner 6: scale animation

        std::cout << "\nLoading Animations initialized!\n";
        std::cout << "Press ESC to quit\n\n";

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

            // Limit to ~60 FPS
            SDL_Delay(16);
        }
    }

    ~LoadingAnimationDemo() {
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

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::SharedPtr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Progress bar nodes (for manual text update)
    flex::Node* progress_fill_ = nullptr;
    flex::Node* progress_text_ = nullptr;

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
                    }
                    break;
            }
        }
    }

    void update(float dt) {
        time_ += dt;

        // Update percentage text based on progress bar cycle (5 seconds)
        if (progress_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(progress_text_)) {
                float progress_cycle = fmod(time_, 5.0f) / 5.0f;
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(0) << (progress_cycle * 100.0f) << "%";
                text->set_content(oss.str());
            }
        }

        // Advance all declarative animations
        instance_->advance(dt);
    }

    void render() {
        // Clear ThorVG canvas
        canvas_->remove();

        // Render flex scene to ThorVG
        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
        flex_renderer_->clear(instance_->scene()->background());
        instance_->render(*flex_renderer_);
        flex_renderer_->end_frame();

        // Draw and sync ThorVG canvas
        canvas_->draw();
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
    std::cout << "===========================================\n";
    std::cout << "Loading Animations Demo - Flex Engine\n";
    std::cout << "===========================================\n\n";

    LoadingAnimationDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
