/*
 * Analog Clock Demo - MVC Refactor
 * Real-time clock with animated hands
 * Separates time data (Model), clock visuals (View), and update logic (Controller).
 */

#include <iostream>
#include <ctime>
#include <vector>
#include <memory>
#include <algorithm>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>

// ============================================================================
// Model - Clock State
// ============================================================================

struct ClockModel {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    float sub_second = 0.0f;

    void update_to_current_time(float dt) {
        time_t now = time(nullptr);
        tm* local_time = localtime(&now);
        
        int prev_sec = seconds;
        hours = local_time->tm_hour % 12;
        minutes = local_time->tm_min;
        seconds = local_time->tm_sec;

        if (seconds != prev_sec) {
            sub_second = 0.0f;
        } else {
            sub_second = std::min(0.999f, sub_second + dt);
        }
    }

    float hour_rotation() const {
        return (hours % 12) * 30.0f + minutes * 0.5f;
    }

    float minute_rotation() const {
        return (minutes % 60) * 6.0f + seconds * 0.1f;
    }

    float second_rotation() const {
        return (seconds % 60) * 6.0f + sub_second * 6.0f;
    }
};

// ============================================================================
// View - UI Presentation
// ============================================================================

class ClockView {
public:
    ClockView(flex::Instance::Ptr instance) : instance_(instance) {
        auto* artboard = instance_->artboard();
        if (!artboard) return;

        hands_group_ = artboard->find("hands");
        hour_hand_ = artboard->find("hourHand");
        minute_hand_ = artboard->find("minuteHand");
        second_hand_ = artboard->find("secondHand");
    }

    void update(const ClockModel& model) {
        // Ensure the hands group is centered on the clock face hub
        if (hands_group_) {
            hands_group_->set_position(0, 0);
        }

        // Authoritatively force positions relative to the group center
        // and apply calculated rotations.
        if (hour_hand_) {
            hour_hand_->set_position(0, 0);
            hour_hand_->set_rotation(model.hour_rotation());
        }
        if (minute_hand_) {
            minute_hand_->set_position(0, 0);
            minute_hand_->set_rotation(model.minute_rotation());
        }
        if (second_hand_) {
            second_hand_->set_position(0, 0);
            second_hand_->set_rotation(model.second_rotation());
        }
    }

private:
    flex::Instance::Ptr instance_;
    flex::Node* hands_group_ = nullptr;
    flex::Node* hour_hand_ = nullptr;
    flex::Node* minute_hand_ = nullptr;
    flex::Node* second_hand_ = nullptr;
};

// ============================================================================
// Controller - Logic and Coordination
// ============================================================================

class ClockController {
public:
    ClockController(ClockModel& model, ClockView& view) : model_(model), view_(view) {}

    void update(float dt) {
        model_.update_to_current_time(dt);
        view_.update(model_);
    }

private:
    ClockModel& model_;
    ClockView& view_;
};

// ============================================================================
// Application Shell
// ============================================================================

class AnalogClockDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Analog Clock - MVC Demo",
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

        auto definition = flex::Definition::load_file("analog_clock.flex");
        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        model_ = std::make_unique<ClockModel>();
        view_ = std::make_unique<ClockView>(instance_);
        controller_ = std::make_unique<ClockController>(*model_, *view_);

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
            
            instance_->advance(dt);
            controller_->update(dt);
            
            render();
            SDL_Delay(16);
        }
    }

    ~AnalogClockDemo() {
        controller_.reset();
        view_.reset();
        model_.reset();
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
    static constexpr int WIDTH = 600;
    static constexpr int HEIGHT = 600;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    std::unique_ptr<ClockModel> model_;
    std::unique_ptr<ClockView> view_;
    std::unique_ptr<ClockController> controller_;

    bool running_ = false;

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: running_ = false; break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) running_ = false;
                    break;
            }
        }
    }

    void render() {
        canvas_->remove();
        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
        flex_renderer_->clear(instance_->artboard()->background());
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
    AnalogClockDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
