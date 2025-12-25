/*
 * Easing & Crossfade Demo
 *
 * Demonstrates:
 * - Cubic Bezier easing functions (Linear, Ease, EaseIn, EaseOut, EaseInOut)
 * - Smooth crossfade between animations
 *
 * Controls:
 * - SPACE: Toggle between bounce and slide animations
 * - ESC: Quit
 */

#include <iostream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

class EasingDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Easing & Crossfade Demo - Press SPACE to switch animations",
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
        canvas_.reset(tvg::SwCanvas::gen());
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

        // Create Flex instance
        instance_ = flex::Instance::create(WIDTH, HEIGHT);
        auto* artboard = instance_->artboard();
        auto* alloc = instance_->object_allocator();

        // Create 5 circles with labels
        const char* easing_names[] = {"Linear", "Ease", "EaseIn", "EaseOut", "EaseInOut"};
        flex::Easing easings[] = {
            flex::Easing::linear(),
            flex::Easing::ease(),
            flex::Easing::ease_in(),
            flex::Easing::ease_out(),
            flex::Easing::ease_in_out()
        };

        float start_x = 100;
        float spacing = 150;
        float start_y = 300;

        for (int i = 0; i < 5; i++) {
            // Create circle
            auto circle = flex::Shape::create();
            circle->set_id(easing_names[i]);
            circle->set_circle(30);
            circle->set_fill(flex::Color(0.2f + i * 0.15f, 0.5f, 0.8f - i * 0.1f, 1.0f));
            circle->set_position(start_x + i * spacing, start_y);
            artboard->add_child(circle);
            circles_[i] = circle.get();

            // Create label
            auto label = flex::Text::create();
            label->set_content(easing_names[i]);
            label->set_font_size(14);
            label->set_color(flex::Color::White);
            label->set_position(start_x + i * spacing - 30, start_y + 60);
            artboard->add_child(label);
        }

        // Create instruction text
        auto instructions = flex::Text::create();
        instructions->set_content("Press SPACE to toggle animations");
        instructions->set_font_size(20);
        instructions->set_color(flex::Color::White);
        instructions->set_position(200, 50);
        artboard->add_child(instructions);

        // Create animation status text
        status_text_ = flex::Text::create();
        status_text_->set_content("Current: Bounce Animation");
        status_text_->set_font_size(18);
        status_text_->set_color(flex::Color(1, 1, 0, 1));
        status_text_->set_position(250, 100);
        artboard->add_child(status_text_);

        // Create bounce animation (y-axis movement)
        // Each circle gets its own timeline with different easing
        for (int i = 0; i < 5; i++) {
            std::string anim_name = std::string("bounce_") + easing_names[i];
            auto bounce_anim = flex::Timeline::create(anim_name.c_str(), *alloc);
            bounce_anim->set_duration(2.0f);
            bounce_anim->set_loop_mode(flex::LoopMode::Loop);

            auto track = bounce_anim->add_track("y");  // ← Correct: property name
            track->add_keyframe(0.0f, start_y, easings[i]);
            track->add_keyframe(1.0f, start_y - 150, easings[i]);
            track->add_keyframe(2.0f, start_y, easings[i]);

            instance_->add_timeline(bounce_anim);
        }

        // Create slide animation (x-axis movement)
        for (int i = 0; i < 5; i++) {
            std::string anim_name = std::string("slide_") + easing_names[i];
            auto slide_anim = flex::Timeline::create(anim_name.c_str(), *alloc);
            slide_anim->set_duration(2.0f);
            slide_anim->set_loop_mode(flex::LoopMode::Loop);

            auto track = slide_anim->add_track("x");  // ← Correct: property name
            float origin_x = start_x + i * spacing;
            track->add_keyframe(0.0f, origin_x, easings[i]);
            track->add_keyframe(1.0f, origin_x + 100, easings[i]);
            track->add_keyframe(2.0f, origin_x, easings[i]);

            instance_->add_timeline(slide_anim);
        }

        // Create ThorVG renderer
        flex_renderer_ = flex::create_thorvg_renderer(canvas_.get());

        // Start with bounce animation
        for (int i = 0; i < 5; i++) {
            std::string anim_name = std::string("bounce_") + easing_names[i];
            auto* player = instance_->play(anim_name.c_str(), circles_[i]);
        }
        current_anim_ = "bounce";

        std::cout << "Easing Demo initialized!\n";
        std::cout << "Watch how each circle moves with different easing:\n";
        std::cout << "  Linear:     Constant speed\n";
        std::cout << "  Ease:       Smooth acceleration and deceleration\n";
        std::cout << "  EaseIn:     Slow start, fast end\n";
        std::cout << "  EaseOut:    Fast start, slow end\n";
        std::cout << "  EaseInOut:  Slow start and end, fast middle\n\n";
        std::cout << "Press SPACE to toggle animations\n";
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

            SDL_Delay(16);  // ~60 FPS
        }
    }

    ~EasingDemo() {
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();

        if (canvas_) canvas_.reset();
        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running_ = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running_ = false;
                        break;
                    case SDLK_SPACE:
                        toggle_animation();
                        break;
                }
            }
        }
    }

    void toggle_animation() {
        // Switch between bounce and slide with crossfade
        const char* new_anim = (current_anim_ == "bounce") ? "slide" : "bounce";

        std::cout << "Crossfading to " << new_anim << " animation (0.5s fade)\n";

        const char* easing_names[] = {"Linear", "Ease", "EaseIn", "EaseOut", "EaseInOut"};

        // Crossfade each circle
        for (int i = 0; i < 5; i++) {
            std::string anim_name = std::string(new_anim) + "_" + easing_names[i];
            instance_->animation_controller()->crossfade(anim_name.c_str(), circles_[i], 0.5f);
        }

        current_anim_ = new_anim;

        // Update status text
        if (current_anim_ == "bounce") {
            status_text_->set_content("Current: Bounce Animation (Y-axis)");
        } else {
            status_text_->set_content("Current: Slide Animation (X-axis)");
        }
    }

    void update(float dt) {
        instance_->advance(dt);
    }

    void render() {
        // Begin frame
        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);

        // Render scene
        instance_->render(*flex_renderer_);

        // End frame (ThorVG draw + sync)
        flex_renderer_->end_frame();

        // Copy to SDL texture
        void* pixels;
        int pitch;
        SDL_LockTexture(texture_, nullptr, &pixels, &pitch);
        memcpy(pixels, buffer_.data(), WIDTH * HEIGHT * 4);
        SDL_UnlockTexture(texture_);

        // Present
        SDL_RenderClear(sdl_renderer_);
        SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer_);
    }

    static const int WIDTH = 900;
    static const int HEIGHT = 600;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    flex::Node* circles_[5] = {nullptr};
    flex::Text::Ptr status_text_;  // Use shared_ptr
    const char* current_anim_ = "bounce";
    bool running_ = false;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    EasingDemo demo;
    if (!demo.init()) {
        std::cerr << "Failed to initialize demo\n";
        return 1;
    }

    demo.run();
    return 0;
}
