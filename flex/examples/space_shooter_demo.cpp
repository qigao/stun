/*
 * Space Shooter Game Demo
 * Vertical scrolling space shooter
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

class SpaceShooterGame {
public:
    bool init() {
        srand(time(nullptr));

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Space Shooter - Flex Engine Demo",
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

        std::cout << "Loading Space Shooter...\n";
        auto definition = flex::Definition::load_file("space_shooter.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find game objects
        auto* artboard = instance_->artboard();
        player_ = artboard->find("player");
        score_text_ = artboard->find("scoreValue");
        health_fill_ = artboard->find("healthFill");
        game_over_screen_ = artboard->find("gameOverScreen");
        final_score_text_ = artboard->find("finalScoreValue");

        // Bullets
        for (int i = 0; i < 10; i++) {
            std::string name = "bullet" + std::to_string(i);
            bullets_.push_back(artboard->find(name));
            bullet_active_.push_back(false);
            bullet_y_.push_back(0.0f);
        }

        // Enemies
        for (int i = 0; i < 4; i++) {
            std::string name = "enemy" + std::to_string(i);
            enemies_.push_back(artboard->find(name));
            enemy_active_.push_back(false);
            enemy_x_.push_back(0.0f);
            enemy_y_.push_back(0.0f);
        }

        // Explosions
        for (int i = 0; i < 4; i++) {
            std::string name = "explosion" + std::to_string(i);
            explosions_.push_back(artboard->find(name));
            explosion_active_.push_back(false);
            explosion_time_.push_back(0.0f);
        }

        std::cout << "Space Shooter initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  ARROW KEYS - Move\n";
        std::cout << "  SPACE      - Shoot\n";
        std::cout << "  ESC        - Quit\n\n";

        reset_game();
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

    ~SpaceShooterGame() {
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
    static constexpr int WIDTH = 400;
    static constexpr int HEIGHT = 700;
    static constexpr float PLAYER_SPEED = 300.0f;
    static constexpr float BULLET_SPEED = 500.0f;
    static constexpr float ENEMY_SPEED = 100.0f;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Game objects
    flex::Node* player_ = nullptr;
    flex::Node* score_text_ = nullptr;
    flex::Node* health_fill_ = nullptr;
    flex::Node* game_over_screen_ = nullptr;
    flex::Node* final_score_text_ = nullptr;

    std::vector<flex::Node*> bullets_;
    std::vector<bool> bullet_active_;
    std::vector<float> bullet_y_;

    std::vector<flex::Node*> enemies_;
    std::vector<bool> enemy_active_;
    std::vector<float> enemy_x_;
    std::vector<float> enemy_y_;

    std::vector<flex::Node*> explosions_;
    std::vector<bool> explosion_active_;
    std::vector<float> explosion_time_;

    bool running_ = false;
    bool game_over_ = false;

    float player_x_ = 200.0f;
    float player_y_ = 600.0f;
    int score_ = 0;
    float health_ = 100.0f;

    float shoot_cooldown_ = 0.0f;
    float enemy_spawn_timer_ = 0.0f;

    void reset_game() {
        player_x_ = 200.0f;
        player_y_ = 600.0f;
        score_ = 0;
        health_ = 100.0f;
        game_over_ = false;
        shoot_cooldown_ = 0.0f;
        enemy_spawn_timer_ = 0.0f;

        for (size_t i = 0; i < bullet_active_.size(); i++) {
            bullet_active_[i] = false;
            if (bullets_[i]) bullets_[i]->set_opacity(0.0f);
        }

        for (size_t i = 0; i < enemy_active_.size(); i++) {
            enemy_active_[i] = false;
            if (enemies_[i]) enemies_[i]->set_opacity(0.0f);
        }

        for (size_t i = 0; i < explosion_active_.size(); i++) {
            explosion_active_[i] = false;
            if (explosions_[i]) explosions_[i]->set_opacity(0.0f);
        }

        if (game_over_screen_) game_over_screen_->set_opacity(0.0f);

        update_ui();
    }

    void update_ui() {
        if (score_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(score_text_)) {
                text->set_content(std::to_string(score_));
            }
        }

        if (health_fill_) {
            float width = (health_ / 100.0f) * 60.0f;
            if (auto* shape = dynamic_cast<flex::Shape*>(health_fill_)) {
                shape->set_rect(width, 10);
            }
        }
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
                    } else if (event.key.keysym.sym == SDLK_SPACE) {
                        if (game_over_) {
                            reset_game();
                        }
                    }
                    break;
            }
        }
    }

    void update(float dt) {
        if (game_over_) return;

        // Player movement
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_LEFT]) {
            player_x_ -= PLAYER_SPEED * dt;
            if (player_x_ < 20) player_x_ = 20;
        }
        if (keys[SDL_SCANCODE_RIGHT]) {
            player_x_ += PLAYER_SPEED * dt;
            if (player_x_ > WIDTH - 20) player_x_ = WIDTH - 20;
        }
        if (keys[SDL_SCANCODE_UP]) {
            player_y_ -= PLAYER_SPEED * dt;
            if (player_y_ < 50) player_y_ = 50;
        }
        if (keys[SDL_SCANCODE_DOWN]) {
            player_y_ += PLAYER_SPEED * dt;
            if (player_y_ > HEIGHT - 50) player_y_ = HEIGHT - 50;
        }

        if (player_) {
            player_->set_x(player_x_);
            player_->set_y(player_y_);
        }

        // Shooting
        shoot_cooldown_ -= dt;
        if (keys[SDL_SCANCODE_SPACE] && shoot_cooldown_ <= 0.0f) {
            shoot_bullet();
            shoot_cooldown_ = 0.2f;
        }

        // Update bullets
        for (size_t i = 0; i < bullets_.size(); i++) {
            if (!bullet_active_[i]) continue;

            bullet_y_[i] -= BULLET_SPEED * dt;

            if (bullet_y_[i] < -10) {
                bullet_active_[i] = false;
                if (bullets_[i]) bullets_[i]->set_opacity(0.0f);
            } else {
                if (bullets_[i]) bullets_[i]->set_y(bullet_y_[i]);
            }
        }

        // Spawn enemies
        enemy_spawn_timer_ += dt;
        if (enemy_spawn_timer_ >= 1.5f) {
            spawn_enemy();
            enemy_spawn_timer_ = 0.0f;
        }

        // Update enemies
        for (size_t i = 0; i < enemies_.size(); i++) {
            if (!enemy_active_[i]) continue;

            enemy_y_[i] += ENEMY_SPEED * dt;

            if (enemy_y_[i] > HEIGHT + 50) {
                enemy_active_[i] = false;
                if (enemies_[i]) enemies_[i]->set_opacity(0.0f);
            } else {
                if (enemies_[i]) {
                    enemies_[i]->set_x(enemy_x_[i]);
                    enemies_[i]->set_y(enemy_y_[i]);
                }

                // Check collision with player
                float dx = enemy_x_[i] - player_x_;
                float dy = enemy_y_[i] - player_y_;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 30) {
                    enemy_active_[i] = false;
                    if (enemies_[i]) enemies_[i]->set_opacity(0.0f);
                    spawn_explosion(enemy_x_[i], enemy_y_[i]);
                    health_ -= 20;
                    update_ui();

                    if (health_ <= 0) {
                        game_over_ = true;
                        if (game_over_screen_) game_over_screen_->set_opacity(1.0f);
                        if (final_score_text_) {
                            if (auto* text = dynamic_cast<flex::Text*>(final_score_text_)) {
                                text->set_content(std::to_string(score_));
                            }
                        }
                    }
                }
            }
        }

        // Check bullet-enemy collisions
        for (size_t i = 0; i < bullets_.size(); i++) {
            if (!bullet_active_[i]) continue;

            for (size_t j = 0; j < enemies_.size(); j++) {
                if (!enemy_active_[j]) continue;

                float dx = bullets_[i]->x() - enemy_x_[j];
                float dy = bullet_y_[i] - enemy_y_[j];
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 20) {
                    bullet_active_[i] = false;
                    enemy_active_[j] = false;
                    if (bullets_[i]) bullets_[i]->set_opacity(0.0f);
                    if (enemies_[j]) enemies_[j]->set_opacity(0.0f);

                    spawn_explosion(enemy_x_[j], enemy_y_[j]);
                    score_ += 100;
                    update_ui();
                    break;
                }
            }
        }

        // Update explosions
        for (size_t i = 0; i < explosions_.size(); i++) {
            if (!explosion_active_[i]) continue;

            explosion_time_[i] += dt;

            if (explosion_time_[i] >= 0.3f) {
                explosion_active_[i] = false;
                if (explosions_[i]) explosions_[i]->set_opacity(0.0f);
            } else {
                float alpha = 1.0f - (explosion_time_[i] / 0.3f);
                if (explosions_[i]) explosions_[i]->set_opacity(alpha);
            }
        }

        instance_->advance(dt);
    }

    void shoot_bullet() {
        for (size_t i = 0; i < bullets_.size(); i++) {
            if (!bullet_active_[i]) {
                bullet_active_[i] = true;
                bullet_y_[i] = player_y_ - 30;
                if (bullets_[i]) {
                    bullets_[i]->set_x(player_x_);
                    bullets_[i]->set_y(bullet_y_[i]);
                    bullets_[i]->set_opacity(1.0f);
                }
                break;
            }
        }
    }

    void spawn_enemy() {
        for (size_t i = 0; i < enemies_.size(); i++) {
            if (!enemy_active_[i]) {
                enemy_active_[i] = true;
                enemy_x_[i] = 50 + rand() % (WIDTH - 100);
                enemy_y_[i] = -30;
                if (enemies_[i]) {
                    enemies_[i]->set_x(enemy_x_[i]);
                    enemies_[i]->set_y(enemy_y_[i]);
                    enemies_[i]->set_opacity(1.0f);
                }
                break;
            }
        }
    }

    void spawn_explosion(float x, float y) {
        for (size_t i = 0; i < explosions_.size(); i++) {
            if (!explosion_active_[i]) {
                explosion_active_[i] = true;
                explosion_time_[i] = 0.0f;
                if (explosions_[i]) {
                    explosions_[i]->set_x(x);
                    explosions_[i]->set_y(y);
                    explosions_[i]->set_opacity(1.0f);
                }
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
    std::cout << "===========================================\n";
    std::cout << "Space Shooter - Flex Engine\n";
    std::cout << "===========================================\n\n";

    SpaceShooterGame game;
    if (!game.init()) {
        return 1;
    }

    game.run();
    return 0;
}
