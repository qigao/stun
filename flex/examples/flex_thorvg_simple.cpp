/*
 * Flex Engine - Simple ThorVG Animation Renderer
 *
 * Simplified version with extensive error checking
 */

#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>
#include <flex/renderer.h>
#include <flex/group.h>
#include <flex/shape.h>
#include <flex/text.h>
#include <flex/artboard.h>
#include <iostream>
#include <memory>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

class SimpleAnimationDemo {
public:
    bool init() {
        std::cout << "Initializing SDL2...\n";
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        std::cout << "Creating SDL window...\n";
        window_ = SDL_CreateWindow(
            "Flex Animation Demo",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH,
            HEIGHT,
            SDL_WINDOW_SHOWN
        );

        if (!window_) {
            std::cerr << "SDL window creation failed: " << SDL_GetError() << "\n";
            return false;
        }

        std::cout << "Getting SDL surface...\n";
        surface_ = SDL_GetWindowSurface(window_);
        if (!surface_) {
            std::cerr << "SDL surface creation failed\n";
            return false;
        }

        std::cout << "Initializing ThorVG...\n";
        if (tvg::Initializer::init(0) != tvg::Result::Success) {
            std::cerr << "ThorVG initialization failed\n";
            return false;
        }

        std::cout << "Creating ThorVG canvas...\n";
        canvas_.reset(tvg::SwCanvas::gen());
        canvas_->target(
            static_cast<uint32_t*>(surface_->pixels),
            surface_->pitch / 4,
            surface_->w,
            surface_->h,
            tvg::ColorSpace::ARGB8888
        );

        std::cout << "Initializing Flex Engine...\n";
        flex::init();

        std::cout << "Creating Flex instance...\n";
        instance_ = flex::Instance::create(WIDTH, HEIGHT);
        if (!instance_) {
            std::cerr << "Failed to create Flex instance\n";
            return false;
        }

        std::cout << "Creating renderer...\n";
        renderer_ = flex::create_thorvg_renderer(canvas_.get());
        if (!renderer_) {
            std::cerr << "Failed to create ThorVG renderer\n";
            return false;
        }

        std::cout << "Creating scene...\n";
        create_scene();

        std::cout << "Creating animations...\n";
        create_animations();

        std::cout << "Initialization complete!\n\n";
        return true;
    }

    void run() {
        std::cout << "Starting main loop...\n";
        std::cout << "Press ESC to quit\n";

        running_ = true;
        Uint32 last_time = SDL_GetTicks();
        int frame = 0;

        while (running_) {
            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;

            handle_events();
            update(dt);
            render();

            // Print frame info every 60 frames
            if (++frame % 60 == 0) {
                std::cout << "Frame " << frame << " - FPS: "
                          << (1000.0f / (current_time - (last_time - dt * 1000))) << "\n";
            }

            SDL_Delay(16);  // ~60 FPS
        }

        std::cout << "Main loop ended\n";
    }

    ~SimpleAnimationDemo() {
        std::cout << "Shutting down...\n";
        renderer_.reset();
        instance_.reset();
        flex::shutdown();
        canvas_.reset();
        tvg::Initializer::term();
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        std::cout << "Shutdown complete\n";
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::unique_ptr<flex::Renderer> renderer_;
    flex::Instance::Ptr instance_;
    bool running_ = false;

    flex::Shape::Ptr player_shape_;
    flex::Shape::Ptr circle1_;
    flex::Shape::Ptr circle2_;

    flex::TimelinePlayer* move_player_ = nullptr;
    flex::TimelinePlayer* fade_player_ = nullptr;

    void create_scene() {
        auto* artboard = instance_->artboard();
        if (!artboard) {
            std::cerr << "Failed to get artboard\n";
            return;
        }

        // Background
        auto bg = flex::Shape::create();
        bg->set_rect(WIDTH, HEIGHT);
        bg->set_fill(flex::Color(0.1f, 0.1f, 0.15f, 1.0f));
        artboard->add_child(bg);

        // Player rectangle
        player_shape_ = flex::Shape::create();
        player_shape_->set_rect(80, 80);
        player_shape_->set_position(100, 350);
        player_shape_->set_fill(flex::Color(0.2f, 0.6f, 0.86f, 1.0f));
        artboard->add_child(player_shape_);

        // Circle 1
        circle1_ = flex::Shape::create();
        circle1_->set_circle(40);
        circle1_->set_position(150, 150);
        circle1_->set_fill(flex::Color(0.91f, 0.3f, 0.24f, 0.8f));
        artboard->add_child(circle1_);

        // Circle 2
        circle2_ = flex::Shape::create();
        circle2_->set_circle(50);
        circle2_->set_position(650, 200);
        circle2_->set_fill(flex::Color(0.95f, 0.61f, 0.07f, 0.8f));
        artboard->add_child(circle2_);

        std::cout << "Scene created\n";
    }

    void create_animations() {
        // Movement animation
        auto move_anim = flex::Timeline::create("Move");
        move_anim->set_loop_mode(flex::LoopMode::Loop);

        auto track_x = move_anim->add_track("x");
        track_x->add_keyframe(0.0f, 100.0f);
        track_x->add_keyframe(1.0f, 500.0f);
        track_x->add_keyframe(2.0f, 100.0f);

        auto track_y = move_anim->add_track("y");
        track_y->add_keyframe(0.0f, 350.0f);
        track_y->add_keyframe(0.5f, 250.0f);
        track_y->add_keyframe(1.0f, 350.0f);
        track_y->add_keyframe(1.5f, 250.0f);
        track_y->add_keyframe(2.0f, 350.0f);

        instance_->add_timeline(move_anim);

        // Fade animation
        auto fade_anim = flex::Timeline::create("Fade");
        fade_anim->set_loop_mode(flex::LoopMode::PingPong);

        auto fade_track = fade_anim->add_track("opacity");
        fade_track->add_keyframe(0.0f, 0.3f);
        fade_track->add_keyframe(0.5f, 1.0f);
        fade_track->add_keyframe(1.0f, 0.3f);

        instance_->add_timeline(fade_anim);

        // Start animations
        if (player_shape_) {
            move_player_ = instance_->play("Move", player_shape_.get());
            std::cout << "Started move animation on player\n";
        }

        if (circle1_) {
            fade_player_ = instance_->play("Fade", circle1_.get());
            std::cout << "Started fade animation on circle1\n";
        }

        std::cout << "Animations created\n";
    }

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running_ = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running_ = false;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    if (move_player_) {
                        if (move_player_->is_playing()) {
                            move_player_->pause();
                            std::cout << "Paused animation\n";
                        } else {
                            move_player_->play();
                            std::cout << "Resumed animation\n";
                        }
                    }
                }
            }
        }
    }

    void update(float dt) {
        instance_->advance(dt);
    }

    void render() {
        // Clear background
        SDL_FillRect(surface_, nullptr, 0xFF1a1f29);

        // Render with ThorVG
        if (renderer_) {
            renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);

            auto* artboard = instance_->artboard();
            if (artboard) {
                artboard->render(*renderer_);
            }

            renderer_->end_frame();
        }

        // Update window
        SDL_UpdateWindowSurface(window_);
    }
};

int main(int argc, char** argv) {
    std::cout << "=== Flex Engine Simple ThorVG Animation Demo ===\n\n";

    SimpleAnimationDemo demo;

    if (!demo.init()) {
        std::cerr << "\nInitialization failed! Exiting.\n";
        return 1;
    }

    demo.run();

    std::cout << "\nDemo finished normally\n";
    return 0;
}
