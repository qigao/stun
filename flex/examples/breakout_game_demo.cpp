/*
 * Breakout Game Demo
 * Classic brick breaker game with physics
 */

#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

class BreakoutGame {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Breakout Game - Flex Engine Demo",
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

        // Create texture
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

        // Load game scene
        std::cout << "Loading Breakout Game...\n";
        auto definition = flex::Definition::load_file("breakout_game.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) {
            std::cerr << "No artboard in definition\n";
            return false;
        }

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find game objects
        auto* artboard = instance_->artboard();
        ball_ = artboard->find("ball");
        paddle_ = artboard->find("paddle");
        score_text_ = artboard->find("scoreValue");
        lives_text_ = artboard->find("livesValue");
        game_over_screen_ = artboard->find("gameOverScreen");
        win_screen_ = artboard->find("winScreen");

        // Find all bricks
        for (int i = 0; i < 50; i++) {
            std::string name = "brick" + std::to_string(i);
            auto* brick = artboard->find(name);
            if (brick) {
                bricks_.push_back(brick);
                brick_alive_.push_back(true);
            }
        }

        std::cout << "Breakout Game initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  LEFT/RIGHT - Move paddle\n";
        std::cout << "  SPACE      - Launch ball / Restart\n";
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

    ~BreakoutGame() {
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
    static constexpr int HEIGHT = 800;
    static constexpr float BALL_SPEED = 300.0f;
    static constexpr float PADDLE_SPEED = 500.0f;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Game objects
    flex::Node* ball_ = nullptr;
    flex::Node* paddle_ = nullptr;
    flex::Node* score_text_ = nullptr;
    flex::Node* lives_text_ = nullptr;
    flex::Node* game_over_screen_ = nullptr;
    flex::Node* win_screen_ = nullptr;
    std::vector<flex::Node*> bricks_;
    std::vector<bool> brick_alive_;

    bool running_ = false;
    bool game_over_ = false;
    bool won_ = false;

    // Game state
    float ball_x_ = 300.0f;
    float ball_y_ = 400.0f;
    float ball_vx_ = 200.0f;
    float ball_vy_ = -300.0f;
    bool ball_launched_ = false;

    float paddle_x_ = 300.0f;
    int score_ = 0;
    int lives_ = 3;

    void reset_game() {
        ball_x_ = 300.0f;
        ball_y_ = 400.0f;
        ball_vx_ = 200.0f;
        ball_vy_ = -300.0f;
        ball_launched_ = false;
        paddle_x_ = 300.0f;
        score_ = 0;
        lives_ = 3;
        game_over_ = false;
        won_ = false;

        // Reset bricks
        for (size_t i = 0; i < brick_alive_.size(); i++) {
            brick_alive_[i] = true;
            if (bricks_[i]) bricks_[i]->set_opacity(1.0f);
        }

        if (game_over_screen_) game_over_screen_->set_opacity(0.0f);
        if (win_screen_) win_screen_->set_opacity(0.0f);

        update_ui();
    }

    void update_ui() {
        if (score_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(score_text_)) {
                text->set_content(std::to_string(score_));
            }
        }

        if (lives_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(lives_text_)) {
                text->set_content(std::to_string(lives_));
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
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running_ = false;
                            break;

                        case SDLK_SPACE:
                            if (game_over_ || won_) {
                                reset_game();
                            } else if (!ball_launched_) {
                                ball_launched_ = true;
                            }
                            break;
                    }
                    break;
            }
        }
    }

    void update(float dt) {
        if (game_over_ || won_) {
            return;
        }

        // Paddle movement
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_LEFT]) {
            paddle_x_ -= PADDLE_SPEED * dt;
            if (paddle_x_ < 60) paddle_x_ = 60;
        }
        if (keys[SDL_SCANCODE_RIGHT]) {
            paddle_x_ += PADDLE_SPEED * dt;
            if (paddle_x_ > WIDTH - 60) paddle_x_ = WIDTH - 60;
        }

        if (paddle_) paddle_->set_x(paddle_x_);

        // Ball follows paddle before launch
        if (!ball_launched_) {
            ball_x_ = paddle_x_;
            ball_y_ = 700.0f;
        } else {
            // Ball physics
            ball_x_ += ball_vx_ * dt;
            ball_y_ += ball_vy_ * dt;

            // Wall collisions
            if (ball_x_ - 8 < 0 || ball_x_ + 8 > WIDTH) {
                ball_vx_ = -ball_vx_;
                ball_x_ = std::max(8.0f, std::min(ball_x_, WIDTH - 8.0f));
            }
            if (ball_y_ - 8 < 0) {
                ball_vy_ = -ball_vy_;
                ball_y_ = 8;
            }

            // Paddle collision
            if (ball_y_ + 8 >= 712 && ball_y_ + 8 <= 728) {
                if (ball_x_ >= paddle_x_ - 60 && ball_x_ <= paddle_x_ + 60) {
                    ball_vy_ = -std::abs(ball_vy_);
                    // Add horizontal velocity based on hit position
                    float hit_pos = (ball_x_ - paddle_x_) / 60.0f;
                    ball_vx_ = hit_pos * 300.0f;
                }
            }

            // Brick collisions
            for (size_t i = 0; i < bricks_.size(); i++) {
                if (!brick_alive_[i]) continue;

                auto* brick = bricks_[i];
                float bx = brick->x();
                float by = brick->y() + 80;  // Offset by bricks group y

                // Simple AABB collision
                if (ball_x_ + 8 >= bx && ball_x_ - 8 <= bx + 50 &&
                    ball_y_ + 8 >= by && ball_y_ - 8 <= by + 20) {

                    brick_alive_[i] = false;
                    brick->set_opacity(0.0f);
                    ball_vy_ = -ball_vy_;
                    score_ += 10;
                    update_ui();

                    // Check win condition
                    bool all_destroyed = true;
                    for (bool alive : brick_alive_) {
                        if (alive) {
                            all_destroyed = false;
                            break;
                        }
                    }
                    if (all_destroyed) {
                        won_ = true;
                        if (win_screen_) win_screen_->set_opacity(1.0f);
                    }
                    break;
                }
            }

            // Ball fell off bottom
            if (ball_y_ > HEIGHT) {
                lives_--;
                update_ui();

                if (lives_ <= 0) {
                    game_over_ = true;
                    if (game_over_screen_) game_over_screen_->set_opacity(1.0f);
                } else {
                    ball_launched_ = false;
                    ball_x_ = paddle_x_;
                    ball_y_ = 700.0f;
                    ball_vx_ = 200.0f;
                    ball_vy_ = -300.0f;
                }
            }
        }

        // Update ball position
        if (ball_) {
            ball_->set_x(ball_x_);
            ball_->set_y(ball_y_);
        }

        instance_->advance(dt);
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
    std::cout << "Breakout Game - Flex Engine\n";
    std::cout << "===========================================\n\n";

    BreakoutGame game;
    if (!game.init()) {
        return 1;
    }

    game.run();
    return 0;
}
