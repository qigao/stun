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
        if (!instance_->artboard()) {
            std::cerr << "No artboard in definition\n";
            return false;
        }

        // Create ThorVG renderer
        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find nodes we need to animate
        auto* artboard = instance_->artboard();

        // Spinner nodes
        spinner1_rotator_ = artboard->find("rotator");
        spinner2_pulse1_ = artboard->find("pulse1");
        spinner2_pulse2_ = artboard->find("pulse2");
        spinner2_pulse3_ = artboard->find("pulse3");

        // Wave dots
        wave_dot1_ = artboard->find("dot1");
        wave_dot2_ = artboard->find("dot2");
        wave_dot3_ = artboard->find("dot3");
        wave_dot4_ = artboard->find("dot4");

        // Progress bar
        progress_fill_ = artboard->find("progress");
        progress_text_ = artboard->find("percentage");

        // Orbit dots
        orbit1_ = artboard->find("orbit1");
        orbit2_ = artboard->find("orbit2");
        orbit3_ = artboard->find("orbit3");

        // Square spinner
        square_ = artboard->find("square");

        // Bouncing bar
        bounce_bar_ = artboard->find("bar");

        std::cout << "Loading Animations initialized!\n";
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

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Animation nodes
    flex::Node* spinner1_rotator_ = nullptr;
    flex::Node* spinner2_pulse1_ = nullptr;
    flex::Node* spinner2_pulse2_ = nullptr;
    flex::Node* spinner2_pulse3_ = nullptr;
    flex::Node* wave_dot1_ = nullptr;
    flex::Node* wave_dot2_ = nullptr;
    flex::Node* wave_dot3_ = nullptr;
    flex::Node* wave_dot4_ = nullptr;
    flex::Node* progress_fill_ = nullptr;
    flex::Node* progress_text_ = nullptr;
    flex::Node* orbit1_ = nullptr;
    flex::Node* orbit2_ = nullptr;
    flex::Node* orbit3_ = nullptr;
    flex::Node* square_ = nullptr;
    flex::Node* bounce_bar_ = nullptr;

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

        // Spinner 1: Continuous rotation
        if (spinner1_rotator_) {
            float rotation = fmod(time_ * 180.0f, 360.0f);  // 2 seconds per rotation
            spinner1_rotator_->set_rotation(rotation);
        }

        // Spinner 2: Pulsing circles
        float pulse_cycle = fmod(time_, 3.0f) / 3.0f;  // 3 second cycle
        float pulse_t = pulse_cycle < 0.5f ? pulse_cycle * 2.0f : (1.0f - pulse_cycle) * 2.0f;

        if (spinner2_pulse1_) spinner2_pulse1_->set_opacity(1.0f);
        if (spinner2_pulse2_) spinner2_pulse2_->set_opacity(0.5f + pulse_t * 0.5f);
        if (spinner2_pulse3_) spinner2_pulse3_->set_opacity(0.2f + pulse_t * 0.3f);

        // Spinner 3: Wave animation
        float wave_offset = time_ * 3.0f;  // Wave speed
        if (wave_dot1_) wave_dot1_->set_y(std::sin(wave_offset) * 20.0f);
        if (wave_dot2_) wave_dot2_->set_y(std::sin(wave_offset + 1.57f) * 20.0f);
        if (wave_dot3_) wave_dot3_->set_y(std::sin(wave_offset + 3.14f) * 20.0f);
        if (wave_dot4_) wave_dot4_->set_y(std::sin(wave_offset + 4.71f) * 20.0f);

        // Progress bar
        float progress_cycle = fmod(time_, 5.0f) / 5.0f;  // 5 second cycle
        float progress_width = progress_cycle * 400.0f;

        if (progress_fill_) {
            if (auto* shape = dynamic_cast<flex::Shape*>(progress_fill_)) {
                shape->set_rect(progress_width, 16);
            }
        }

        if (progress_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(progress_text_)) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(0) << (progress_cycle * 100.0f) << "%";
                text->set_content(oss.str());
            }
        }

        // Spinner 4: Orbiting dots (rotate the parent group)
        float orbit_rotation = fmod(time_ * 240.0f, 360.0f);  // 1.5 seconds per rotation
        if (orbit1_) {
            // Rotate each orbit individually for staggered effect
            if (auto* parent = orbit1_->parent()) {
                parent->set_rotation(orbit_rotation);
            }
        }

        // Spinner 5: Square rotation (slower)
        if (square_) {
            float square_rotation = fmod(time_ * 120.0f, 360.0f);  // 3 seconds per rotation
            square_->set_rotation(square_rotation);
        }

        // Spinner 6: Bouncing bar
        if (bounce_bar_) {
            float bounce_cycle = fmod(time_, 1.2f) / 1.2f;  // 1.2 second cycle
            float bounce_t = bounce_cycle < 0.5f ? bounce_cycle * 2.0f : (1.0f - bounce_cycle) * 2.0f;
            float bounce_y = -40.0f + bounce_t * 60.0f;
            bounce_bar_->set_y(bounce_y);
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
