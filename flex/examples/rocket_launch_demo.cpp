/*
 * Rocket Launch Demo
 * Demonstrates path animation with rocket following a launch trajectory
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>
#include <flex/path.h>

class RocketLaunchDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Rocket Launch - Path Animation Demo",
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

        std::cout << "Loading Rocket Launch Demo...\n";
        auto definition = flex::Definition::load_file("rocket_launch.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find UI elements
        auto* artboard = instance_->artboard();

        rocket_ = artboard->find("rocket");
        flame1_ = artboard->find("rocket")->find("flame1");
        flame2_ = artboard->find("rocket")->find("flame2");

        progress_value_ = artboard->find("progress")->find("progressValue");
        altitude_value_ = artboard->find("altitude")->find("altitudeValue");
        speed_value_ = artboard->find("speed")->find("speedValue");

        // Create launch path
        // Path: Vertical launch → Curve right → Space
        launch_path_.add_point(130, 500, 0.0f);    // Start: Launch pad
        launch_path_.add_point(130, 400, 0.2f);    // Vertical ascent
        launch_path_.add_point(150, 300, 0.4f);    // Start curve
        launch_path_.add_point(200, 200, 0.6f);    // Continue curve
        launch_path_.add_point(300, 120, 0.8f);    // Steeper curve
        launch_path_.add_point(450, 50, 1.0f);     // Exit to space

        // Default to smooth Catmull-Rom interpolation
        launch_path_.set_interpolation_mode(flex::Path::InterpolationMode::CatmullRom);

        // Create path animation (8 second launch)
        path_animation_ = std::make_unique<flex::PathAnimation>(&launch_path_, 8.0f);

        std::cout << "Rocket Launch Demo initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  SPACE - Launch rocket\n";
        std::cout << "  R - Reset\n";
        std::cout << "  M - Toggle interpolation mode (Linear/Smooth)\n";
        std::cout << "  ESC - Quit\n\n";

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

    ~RocketLaunchDemo() {
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
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Path animation
    flex::Path launch_path_;
    std::unique_ptr<flex::PathAnimation> path_animation_;

    // UI elements
    flex::Node* rocket_ = nullptr;
    flex::Node* flame1_ = nullptr;
    flex::Node* flame2_ = nullptr;
    flex::Node* progress_value_ = nullptr;
    flex::Node* altitude_value_ = nullptr;
    flex::Node* speed_value_ = nullptr;

    bool running_ = false;
    bool launched_ = false;

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
                        launch();
                    } else if (event.key.keysym.sym == SDLK_r) {
                        reset();
                    } else if (event.key.keysym.sym == SDLK_m) {
                        toggle_interpolation_mode();
                    }
                    break;
            }
        }
    }

    void launch() {
        if (!launched_) {
            launched_ = true;
            path_animation_->play();
            std::cout << "🚀 Rocket launch initiated!\n";
        }
    }

    void reset() {
        launched_ = false;
        path_animation_->stop();

        if (rocket_) {
            rocket_->set_position(130, 500);
        }

        update_ui();

        // Hide flames
        if (flame1_) flame1_->set_opacity(0.0f);
        if (flame2_) flame2_->set_opacity(0.0f);

        std::cout << "🔄 Reset to launch pad\n";
    }

    void toggle_interpolation_mode() {
        if (launch_path_.interpolation_mode() == flex::Path::InterpolationMode::Linear) {
            launch_path_.set_interpolation_mode(flex::Path::InterpolationMode::CatmullRom);
            std::cout << "🎨 Switched to Smooth (Catmull-Rom) interpolation\n";
        } else {
            launch_path_.set_interpolation_mode(flex::Path::InterpolationMode::Linear);
            std::cout << "📐 Switched to Linear interpolation\n";
        }

        // Reset to see the difference
        reset();
    }

    void update(float dt) {
        if (path_animation_->is_playing()) {
            // Update path animation
            auto pos = path_animation_->update(dt);

            // Update rocket position
            if (rocket_) {
                rocket_->set_position(pos.x, pos.y);
            }

            // Show flames during launch
            if (flame1_) flame1_->set_opacity(1.0f);
            if (flame2_) flame2_->set_opacity(1.0f);

            update_ui();

            // Check if finished
            if (path_animation_->is_finished()) {
                std::cout << "✅ Rocket reached orbit!\n";
                launched_ = false;

                // Hide flames
                if (flame1_) flame1_->set_opacity(0.0f);
                if (flame2_) flame2_->set_opacity(0.0f);
            }
        }

        instance_->advance(dt);
    }

    void update_ui() {
        float progress = path_animation_->progress();

        // Update progress percentage
        if (auto* text = dynamic_cast<flex::Text*>(progress_value_)) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << (progress * 100) << "%";
            text->set_content(oss.str());
        }

        // Update altitude (0-400 km)
        if (auto* text = dynamic_cast<flex::Text*>(altitude_value_)) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << (progress * 400) << " km";
            text->set_content(oss.str());
        }

        // Update speed (0-7.8 km/s for orbital velocity)
        if (auto* text = dynamic_cast<flex::Text*>(speed_value_)) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << (progress * 7.8) << " km/s";
            text->set_content(oss.str());
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
    std::cout << "===========================================\n";
    std::cout << "Rocket Launch Demo - Path Animation\n";
    std::cout << "===========================================\n\n";

    RocketLaunchDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
