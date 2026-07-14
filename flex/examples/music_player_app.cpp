/*
 * Music Player Demo
 * Modern music player using Flex DSL with MVC pattern
 *
 * Features:
 * - Loads UI from music_player.flex
 * - Animated visualizer bars
 * - Progress bar with seek
 * - Play/pause toggle
 * - Volume control
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <memory>
#include <random>
#include <chrono>

#include <SDL2/SDL.h>
#include <thorvg.h>

#include <flex.h>
#include "backends/thorvg/init.h"
#include <flex/core/group.h>
#include <flex/core/shape.h>
#include <flex/core/text.h>

// ============================================================================
// Utilities
// ============================================================================

static std::string format_time(float seconds) {
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    std::ostringstream oss;
    oss << mins << ":" << std::setfill('0') << std::setw(2) << secs;
    return oss.str();
}

// ============================================================================
// Model - Music Player State
// ============================================================================

struct Song {
    std::string title = "Midnight Dreams";
    std::string artist = "Synthwave Collective";
    std::string album = "Neon Horizons";
    float duration = 245.0f;  // 4:05
};

class MusicPlayerModel {
public:
    Song song;
    float current_time = 0.0f;
    float volume = 0.75f;
    bool is_playing = false;

    // Visualizer levels (0-100)
    float visualizer_levels[20] = {
        45, 65, 85, 70, 95, 80, 60, 90, 75, 55,
        70, 85, 65, 95, 50, 80, 70, 60, 45, 35
    };

    std::mt19937 rng{std::random_device{}()};

    float progress() const {
        return song.duration > 0 ? current_time / song.duration : 0;
    }

    void toggle_play() {
        is_playing = !is_playing;
        std::cout << (is_playing ? "▶ Playing" : "⏸ Paused") << std::endl;
    }

    void seek(float progress) {
        current_time = std::max(0.0f, std::min(1.0f, progress)) * song.duration;
    }

    void set_volume(float vol) {
        volume = std::max(0.0f, std::min(1.0f, vol));
        std::cout << "🔊 Volume: " << static_cast<int>(volume * 100) << "%" << std::endl;
    }

    void update(float dt) {
        if (is_playing) {
            current_time += dt;
            if (current_time >= song.duration) {
                current_time = 0.0f;
            }

            // Update visualizer with random variation
            std::uniform_real_distribution<float> dist(-15.0f, 15.0f);
            for (int i = 0; i < 20; ++i) {
                float base = 50.0f + 30.0f * std::sin(i * 0.5f + current_time * 2.0f);
                visualizer_levels[i] = std::max(10.0f, std::min(100.0f, base + dist(rng)));
            }
        }
    }
};

// ============================================================================
// View - UI Binding
// ============================================================================

class MusicPlayerView {
public:
    MusicPlayerView(flex::Instance::SharedPtr instance) : instance_(instance) {
        auto* scene = instance_->scene();
        if (!scene) return;

        // Find visualizer bars
        auto* visualizer = scene->find("visualizer_section");
        if (visualizer && visualizer->is_group()) {
            auto* group = static_cast<flex::Group*>(visualizer);
            const auto& children = group->children();
            for (size_t i = 0; i < children.size() && i < 20; ++i) {
                visualizer_bars_[i] = dynamic_cast<flex::Shape*>(children[i]);
            }
        }

        // Find progress elements
        track_fill_ = dynamic_cast<flex::Shape*>(scene->find("track_fill"));
        playhead_ = dynamic_cast<flex::Shape*>(scene->find("playhead"));
        time_current_ = dynamic_cast<flex::Text*>(scene->find("time_current"));
        time_total_ = dynamic_cast<flex::Text*>(scene->find("time_total"));

        // Find volume elements
        vol_fill_ = dynamic_cast<flex::Shape*>(scene->find("vol_fill"));
        vol_thumb_ = dynamic_cast<flex::Shape*>(scene->find("vol_thumb"));
        volume_value_ = dynamic_cast<flex::Text*>(scene->find("volume_value"));

        // Find play button elements
        pause_bar1_ = scene->find("pause_bar1");
        pause_bar2_ = scene->find("pause_bar2");

        // Find disc for rotation
        disc_ = scene->find("disc");

        // Find glow for pulse
        album_glow_ = scene->find("album_glow");
    }

    void update(const MusicPlayerModel& model, float dt) {
        // Update visualizer bars
        const float max_height = 60.0f;
        for (int i = 0; i < 20; ++i) {
            if (visualizer_bars_[i]) {
                float level = model.visualizer_levels[i];
                float height = level / 100.0f * max_height;
                float y = max_height - height;  // Bottom-aligned
                visualizer_bars_[i]->set_y(y);
                visualizer_bars_[i]->set_rect(8, height, 0);
            }
        }

        // Update progress bar
        const float progress_width = 340.0f;
        float progress = model.progress();
        if (track_fill_) {
            track_fill_->set_rect(progress * progress_width, 6, 3);
        }
        if (playhead_) {
            playhead_->set_x(progress * progress_width);
        }
        if (time_current_) {
            time_current_->set_content(format_time(model.current_time));
        }
        if (time_total_) {
            time_total_->set_content(format_time(model.song.duration));
        }

        // Update volume
        const float vol_width = 120.0f;
        if (vol_fill_) {
            vol_fill_->set_rect(model.volume * vol_width, 4, 2);
        }
        if (vol_thumb_) {
            vol_thumb_->set_x(model.volume * vol_width);
        }
        if (volume_value_) {
            volume_value_->set_content(std::to_string(static_cast<int>(model.volume * 100)) + "%");
        }

        // Update play/pause icon visibility
        if (pause_bar1_ && pause_bar2_) {
            pause_bar1_->set_visible(model.is_playing);
            pause_bar2_->set_visible(model.is_playing);
        }

        // Rotate disc when playing
        if (disc_ && model.is_playing) {
            disc_rotation_ += dt * 45.0f;  // 45 degrees per second
            if (disc_rotation_ >= 360.0f) disc_rotation_ -= 360.0f;
            disc_->set_rotation(disc_rotation_);
        }

        // Pulse glow when playing
        if (album_glow_ && model.is_playing) {
            glow_time_ += dt;
            float glow_alpha = 0.15f + 0.1f * std::sin(glow_time_ * 2.0f);
            album_glow_->set_opacity(glow_alpha);
        }
    }

private:
    flex::Instance::SharedPtr instance_;
    flex::Shape* visualizer_bars_[20] = {};
    flex::Shape* track_fill_ = nullptr;
    flex::Shape* playhead_ = nullptr;
    flex::Text* time_current_ = nullptr;
    flex::Text* time_total_ = nullptr;
    flex::Shape* vol_fill_ = nullptr;
    flex::Shape* vol_thumb_ = nullptr;
    flex::Text* volume_value_ = nullptr;
    flex::Node* pause_bar1_ = nullptr;
    flex::Node* pause_bar2_ = nullptr;
    flex::Node* disc_ = nullptr;
    flex::Node* album_glow_ = nullptr;
    float disc_rotation_ = 0.0f;
    float glow_time_ = 0.0f;
};

// ============================================================================
// Controller - Input Handling
// ============================================================================

class MusicPlayerController {
public:
    MusicPlayerController(MusicPlayerModel& model) : model_(model) {}

    void handle_key(SDL_Keycode key) {
        switch (key) {
            case SDLK_SPACE:
                model_.toggle_play();
                break;
            case SDLK_LEFT:
                model_.seek(model_.progress() - 0.05f);
                break;
            case SDLK_RIGHT:
                model_.seek(model_.progress() + 0.05f);
                break;
            case SDLK_UP:
                model_.set_volume(model_.volume + 0.1f);
                break;
            case SDLK_DOWN:
                model_.set_volume(model_.volume - 0.1f);
                break;
        }
    }

    void handle_click(float x, float y) {
        // Check progress bar area (approximate)
        if (y >= 500 && y <= 550 && x >= 40 && x <= 380) {
            float progress = (x - 40.0f) / 340.0f;
            model_.seek(progress);
        }
        // Check play button area
        if (y >= 560 && y <= 640) {
            float center_x = 210.0f;
            float dx = x - center_x;
            if (dx * dx < 35 * 35) {
                model_.toggle_play();
            }
        }
    }

private:
    MusicPlayerModel& model_;
};

// ============================================================================
// Application
// ============================================================================

class MusicPlayerApp {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Flex Music Player - Space:Play/Pause Arrow:Seek Up/Down:Volume",
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

        // Load fonts
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        // Load the .flex file
        auto definition = flex::Definition::load_file("music_player.flex");
        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) {
            std::cerr << "Failed to create scene\n";
            return false;
        }

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Initialize MVC
        model_ = std::make_unique<MusicPlayerModel>();
        view_ = std::make_unique<MusicPlayerView>(instance_);
        controller_ = std::make_unique<MusicPlayerController>(*model_);

        std::cout << "🎵 Music Player initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  Space     - Play/Pause\n";
        std::cout << "  Left/Right - Seek -/+ 5 seconds\n";
        std::cout << "  Up/Down   - Volume +/- 10%\n";
        std::cout << "  ESC       - Quit\n";

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

            model_->update(dt);
            view_->update(*model_, dt);
            instance_->advance(dt);

            render();
            SDL_Delay(16);
        }
    }

    ~MusicPlayerApp() {
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
    static constexpr int WIDTH = 420;
    static constexpr int HEIGHT = 720;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::SharedPtr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    std::unique_ptr<MusicPlayerModel> model_;
    std::unique_ptr<MusicPlayerView> view_;
    std::unique_ptr<MusicPlayerController> controller_;

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
                    } else {
                        controller_->handle_key(event.key.keysym.sym);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    controller_->handle_click(
                        static_cast<float>(event.button.x),
                        static_cast<float>(event.button.y)
                    );
                    break;
            }
        }
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
    (void)argc;
    (void)argv;

    MusicPlayerApp app;
    if (!app.init()) {
        std::cerr << "Failed to initialize app\n";
        return 1;
    }
    app.run();
    return 0;
}
