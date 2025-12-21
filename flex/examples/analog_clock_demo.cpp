/*
 * Analog Clock Demo
 * Real-time clock with animated hands
 */

#include <iostream>
#include <ctime>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>

class AnalogClockDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Analog Clock - Flex Engine Demo",
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

        // Load clock
        std::cout << "Loading Analog Clock...\n";
        auto definition = flex::Definition::load_file("analog_clock.flex");

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

        // Find clock hands
        hour_hand_ = instance_->artboard()->find("hourHand");
        minute_hand_ = instance_->artboard()->find("minuteHand");
        second_hand_ = instance_->artboard()->find("secondHand");

        if (!hour_hand_) std::cerr << "Warning: Could not find 'hourHand' node\n";
        if (!minute_hand_) std::cerr << "Warning: Could not find 'minuteHand' node\n";
        if (!second_hand_) std::cerr << "Warning: Could not find 'secondHand' node\n";

        std::cout << "Analog Clock initialized!\n";
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
        }
    }

    ~AnalogClockDemo() {
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
    static constexpr int WIDTH = 600;
    static constexpr int HEIGHT = 600;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    flex::Node* hour_hand_ = nullptr;
    flex::Node* minute_hand_ = nullptr;
    flex::Node* second_hand_ = nullptr;

    bool running_ = false;

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
        // Get current time
        time_t now = time(nullptr);
        tm* local_time = localtime(&now);

        int hours = local_time->tm_hour % 12;  // 12-hour format
        int minutes = local_time->tm_min;
        int seconds = local_time->tm_sec;

        // Calculate rotations
        // Second hand: 6 degrees per second (360 / 60)
        float second_rotation = seconds * 6.0f;

        // Minute hand: 6 degrees per minute + smooth sub-minute movement
        float minute_rotation = minutes * 6.0f + seconds * 0.1f;

        // Hour hand: 30 degrees per hour + smooth sub-hour movement
        float hour_rotation = hours * 30.0f + minutes * 0.5f;

        // Update rotations
        if (second_hand_) second_hand_->set_rotation(second_rotation);
        if (minute_hand_) minute_hand_->set_rotation(minute_rotation);
        if (hour_hand_) hour_hand_->set_rotation(hour_rotation);

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
    std::cout << "Analog Clock Demo - Flex Engine\n";
    std::cout << "===========================================\n\n";

    AnalogClockDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
