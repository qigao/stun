/*
 * Tesla Speedometer Demo
 * Simulates acceleration and displays real-time speed
 */

#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>

class TeslaSpeedometerDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Tesla Speedometer - Flex Engine Demo",
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

        // Load speedometer
        std::cout << "Loading Tesla Speedometer...\n";
        auto definition = flex::Definition::load_file("tesla_speedometer.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) {
            std::cerr << "No artboard in definition\n";
            return false;
        }

        // Create ThorVG renderer
        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find nodes we need to animate
        needle_ = instance_->artboard()->find("needle");
        speed_text_ = instance_->artboard()->find("speedNumber");

        if (!needle_) {
            std::cerr << "Warning: Could not find 'needle' node\n";
        }
        if (!speed_text_) {
            std::cerr << "Warning: Could not find 'speedNumber' node\n";
        }

        std::cout << "Tesla Speedometer initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  UP ARROW    - Accelerate\n";
        std::cout << "  DOWN ARROW  - Brake\n";
        std::cout << "  SPACE       - Auto demo (0-200-0)\n";
        std::cout << "  ESC         - Quit\n\n";

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
        }
    }

    ~TeslaSpeedometerDemo() {
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

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    flex::Node* needle_ = nullptr;
    flex::Node* speed_text_ = nullptr;

    bool running_ = false;
    float time_ = 0;

    // Speed control
    float current_speed_ = 0.0f;     // km/h
    float target_speed_ = 0.0f;      // km/h
    bool auto_demo_ = false;
    float demo_time_ = 0.0f;

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running_ = false;
                            break;

                        case SDLK_UP:
                            // Accelerate
                            auto_demo_ = false;
                            target_speed_ = std::min(200.0f, target_speed_ + 20.0f);
                            std::cout << "Target speed: " << target_speed_ << " km/h\n";
                            break;

                        case SDLK_DOWN:
                            // Brake
                            auto_demo_ = false;
                            target_speed_ = std::max(0.0f, target_speed_ - 20.0f);
                            std::cout << "Target speed: " << target_speed_ << " km/h\n";
                            break;

                        case SDLK_SPACE:
                            // Auto demo: 0 -> 200 -> 0
                            std::cout << "Starting auto demo: 0 -> 200 -> 0\n";
                            auto_demo_ = true;
                            demo_time_ = 0.0f;
                            current_speed_ = 0.0f;
                            break;
                    }
                    break;
            }
        }
    }

    void update(float dt) {
        time_ += dt;

        // Auto demo mode
        if (auto_demo_) {
            demo_time_ += dt;
            float cycle = 10.0f; // 10 second cycle

            if (demo_time_ < cycle / 2) {
                // Accelerate: 0 -> 200 in 5 seconds
                float t = demo_time_ / (cycle / 2);
                // Smooth acceleration (ease-in-out)
                t = t * t * (3.0f - 2.0f * t);
                target_speed_ = 200.0f * t;
            } else {
                // Decelerate: 200 -> 0 in 5 seconds
                float t = (demo_time_ - cycle / 2) / (cycle / 2);
                t = t * t * (3.0f - 2.0f * t);
                target_speed_ = 200.0f * (1.0f - t);
            }

            if (demo_time_ >= cycle) {
                auto_demo_ = false;
                target_speed_ = 0.0f;
                std::cout << "Auto demo completed\n";
            }
        }

        // Smooth speed transition
        float speed_diff = target_speed_ - current_speed_;
        float acceleration = 50.0f; // km/h per second

        if (std::abs(speed_diff) < 0.5f) {
            current_speed_ = target_speed_;
        } else {
            float change = std::copysign(acceleration * dt, speed_diff);
            if (std::abs(change) > std::abs(speed_diff)) {
                current_speed_ = target_speed_;
            } else {
                current_speed_ += change;
            }
        }

        // Update needle rotation
        // Speed range: 0-200 km/h maps to rotation: -135 to +135 degrees (270 degree range)
        // 1 km/h = 1.35 degrees
        if (needle_) {
            float rotation = -135.0f + (current_speed_ * 1.35f);
            needle_->set_rotation(rotation);
        }

        // Update speed text
        if (speed_text_) {
            auto* text = static_cast<flex::Text*>(speed_text_);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << current_speed_;
            text->set_content(oss.str());
        }

        // Advance animations
        instance_->advance(dt);
    }

    void render() {
        // Clear ThorVG canvas
        canvas_->remove();

        // Render flex scene to ThorVG
        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
        flex_renderer_->clear(instance_->artboard()->background());
        instance_->render(*flex_renderer_);
        flex_renderer_->end_frame();

        // Draw and sync ThorVG canvas
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
    std::cout << "===========================================\n";
    std::cout << "Tesla Speedometer Demo - Flex Engine\n";
    std::cout << "===========================================\n\n";

    TeslaSpeedometerDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
