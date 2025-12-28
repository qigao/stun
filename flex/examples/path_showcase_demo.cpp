/*
 * Path Animation Showcase Demo
 * Demonstrates multiple real-world path animation use cases
 */

#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h" 

class PathShowcaseDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Path Animation Showcase",
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

        std::cout << "Loading Path Animation Showcase...\n";
        auto definition = flex::Definition::load_file("path_showcase.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find UI elements
        auto* scene = instance_->scene();

        // Scene 1: Enemy
        enemy_ = scene->find("scene1")->find("enemy");

        // Scene 2: Menu
        menu_ = scene->find("scene2")->find("menu");

        // Scene 3: Particles
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            std::string name = "particle" + std::to_string(i);
            particles_[i] = scene->find("scene3")->find(name);
        }

        // Scene 4: Camera
        camera_ = scene->find("scene4")->find("viewport")->find("camera");

        setup_paths();

        std::cout << "Path Animation Showcase initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  1 - Trigger enemy patrol\n";
        std::cout << "  2 - Trigger menu slide-in\n";
        std::cout << "  3 - Trigger particle flow\n";
        std::cout << "  4 - Trigger camera pan\n";
        std::cout << "  R - Reset all\n";
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
            SDL_Delay(16);  // Limit to ~60 FPS
        }
    }

    ~PathShowcaseDemo() {
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
    static constexpr int WIDTH = 1000;
    static constexpr int HEIGHT = 700;
    static constexpr int PARTICLE_COUNT = 10;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Paths
    flex::Path patrol_path_;
    flex::Path menu_path_;
    flex::Path particle_path_;
    flex::Path camera_path_;

    // Animations
    std::unique_ptr<flex::PathAnimation> patrol_anim_;
    std::unique_ptr<flex::PathAnimation> menu_anim_;
    std::vector<std::unique_ptr<flex::PathAnimation>> particle_anims_;
    std::unique_ptr<flex::PathAnimation> camera_anim_;

    // UI elements
    flex::Node* enemy_ = nullptr;
    flex::Node* menu_ = nullptr;
    flex::Node* particles_[PARTICLE_COUNT];
    flex::Node* camera_ = nullptr;

    bool running_ = false;

    // Particle spawn state
    float particle_spawn_timer_ = 0.0f;
    int next_particle_ = 0;

    void setup_paths() {
        // Scene 1: Enemy patrol (rectangular loop)
        patrol_path_.add_point(100, 150, 0.0f);    // Top-left
        patrol_path_.add_point(400, 150, 0.25f);   // Top-right
        patrol_path_.add_point(400, 220, 0.5f);    // Bottom-right
        patrol_path_.add_point(100, 220, 0.75f);   // Bottom-left
        patrol_path_.add_point(100, 150, 1.0f);    // Back to start
        patrol_path_.set_interpolation_mode(flex::Path::InterpolationMode::CatmullRom);

        patrol_anim_ = std::make_unique<flex::PathAnimation>(&patrol_path_, 6.0f);
        patrol_anim_->set_loop(true);

        // Scene 2: Menu slide-in (with bounce effect)
        menu_path_.add_point(-200, 140, 0.0f);     // Off-screen left
        menu_path_.add_point(100, 140, 0.6f);      // Overshoot right
        menu_path_.add_point(70, 140, 0.8f);       // Bounce back
        menu_path_.add_point(80, 140, 1.0f);       // Final position
        menu_path_.set_interpolation_mode(flex::Path::InterpolationMode::CatmullRom);

        menu_anim_ = std::make_unique<flex::PathAnimation>(&menu_path_, 1.2f);

        // Scene 3: Particle flow (S-curve)
        particle_path_.add_point(60, 150, 0.0f);   // Start
        particle_path_.add_point(150, 100, 0.3f);  // Up
        particle_path_.add_point(250, 180, 0.5f);  // Down
        particle_path_.add_point(350, 120, 0.7f);  // Up again
        particle_path_.add_point(440, 150, 1.0f);  // End
        particle_path_.set_interpolation_mode(flex::Path::InterpolationMode::CatmullRom);

        // Create animations for each particle
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            auto anim = std::make_unique<flex::PathAnimation>(&particle_path_, 3.0f);
            particle_anims_.push_back(std::move(anim));
        }

        // Scene 4: Camera pan (smooth pan across scene)
        camera_path_.add_point(50, 50, 0.0f);      // Start left
        camera_path_.add_point(100, 60, 0.3f);     // Pan right, slight tilt
        camera_path_.add_point(180, 50, 0.6f);     // Continue right
        camera_path_.add_point(250, 55, 1.0f);     // End right
        camera_path_.set_interpolation_mode(flex::Path::InterpolationMode::CatmullRom);

        camera_anim_ = std::make_unique<flex::PathAnimation>(&camera_path_, 4.0f);
    }

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
                    } else if (event.key.keysym.sym == SDLK_1) {
                        trigger_patrol();
                    } else if (event.key.keysym.sym == SDLK_2) {
                        trigger_menu();
                    } else if (event.key.keysym.sym == SDLK_3) {
                        trigger_particles();
                    } else if (event.key.keysym.sym == SDLK_4) {
                        trigger_camera();
                    } else if (event.key.keysym.sym == SDLK_r) {
                        reset_all();
                    }
                    break;
            }
        }
    }

    void trigger_patrol() {
        if (!patrol_anim_->is_playing()) {
            patrol_anim_->play();
            std::cout << "🔄 Enemy patrol started (looping)\n";
        }
    }

    void trigger_menu() {
        menu_anim_->reset();
        menu_anim_->play();
        std::cout << "📱 Menu slide-in triggered (elastic bounce)\n";
    }

    void trigger_particles() {
        particle_spawn_timer_ = 0.0f;
        next_particle_ = 0;
        std::cout << "✨ Particle flow triggered (10 particles)\n";
    }

    void trigger_camera() {
        camera_anim_->reset();
        camera_anim_->play();
        std::cout << "🎥 Camera pan triggered (smooth movement)\n";
    }

    void reset_all() {
        patrol_anim_->stop();
        menu_anim_->stop();
        camera_anim_->stop();

        // Reset enemy
        if (enemy_) enemy_->set_position(100, 150);

        // Reset menu
        if (menu_) menu_->set_position(-200, 140);

        // Reset particles
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            particle_anims_[i]->stop();
            if (particles_[i]) {
                particles_[i]->set_opacity(0.0f);
            }
        }

        // Reset camera
        if (camera_) camera_->set_position(50, 50);

        particle_spawn_timer_ = 0.0f;
        next_particle_ = 0;

        std::cout << "🔄 Reset all animations\n";
    }

    void update(float dt) {
        // Scene 1: Enemy patrol
        if (patrol_anim_->is_playing()) {
            auto pos = patrol_anim_->update(dt);
            if (enemy_) {
                enemy_->set_position(pos.x, pos.y);
            }
        }

        // Scene 2: Menu slide-in
        if (menu_anim_->is_playing()) {
            auto pos = menu_anim_->update(dt);
            if (menu_) {
                menu_->set_position(pos.x, pos.y);
            }
        }

        // Scene 3: Particle flow (spawn particles over time)
        if (next_particle_ < PARTICLE_COUNT) {
            particle_spawn_timer_ += dt;

            // Spawn new particle every 0.3 seconds
            if (particle_spawn_timer_ >= 0.3f) {
                particle_spawn_timer_ = 0.0f;

                if (particles_[next_particle_]) {
                    particles_[next_particle_]->set_opacity(1.0f);
                    particle_anims_[next_particle_]->reset();
                    particle_anims_[next_particle_]->play();
                }

                next_particle_++;
            }
        }

        // Update all active particles
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            if (particle_anims_[i]->is_playing()) {
                auto pos = particle_anims_[i]->update(dt);
                if (particles_[i]) {
                    particles_[i]->set_position(pos.x, pos.y);

                    // Fade out at the end
                    float progress = particle_anims_[i]->progress();
                    if (progress > 0.8f) {
                        float fade = (1.0f - progress) / 0.2f;
                        particles_[i]->set_opacity(fade);
                    }
                }

                // Hide when finished
                if (particle_anims_[i]->is_finished()) {
                    if (particles_[i]) {
                        particles_[i]->set_opacity(0.0f);
                    }
                }
            }
        }

        // Scene 4: Camera pan
        if (camera_anim_->is_playing()) {
            auto pos = camera_anim_->update(dt);
            if (camera_) {
                camera_->set_position(pos.x, pos.y);
            }
        }

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
    std::cout << "Path Animation Showcase\n";
    std::cout << "===========================================\n\n";

    PathShowcaseDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
