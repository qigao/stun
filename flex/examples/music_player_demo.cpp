/*
 * Music Player UI Demo
 * Modern music player interface with animated visualizer
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>

class MusicPlayerDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Music Player - Flex Engine Demo",
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

        // Load music player UI
        std::cout << "Loading Music Player UI...\n";
        auto definition = flex::Definition::load_file("music_player.flex");

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

        // Find nodes we need to animate
        auto* artboard = instance_->artboard();

        // Album art
        album_cover_ = artboard->find("cover");

        // Visualizer bars
        for (int i = 1; i <= 9; i++) {
            std::string name = "bar" + std::to_string(i);
            auto* bar = artboard->find(name);
            if (bar) {
                visualizer_bars_.push_back(bar);
            }
        }

        // Progress bar
        progress_fill_ = artboard->find("progress");
        progress_playhead_ = artboard->find("playhead");
        current_time_text_ = artboard->find("currentTime");

        // UI Interactions
        auto toggle_play = [this]() {
            playing_ = !playing_;
            std::cout << (playing_ ? "▶ Playing (Click)" : "⏸ Paused (Click)") << "\n";
        };

        if (auto* play_btn = artboard->find("playBg")) {
            play_btn->on_click(toggle_play);
        }
        if (auto* play_icon = artboard->find("playTriangle")) {
            play_icon->on_click(toggle_play);
        }
        if (auto* pause_icon = artboard->find("playPause")) { // Just in case it's named differently
            pause_icon->on_click(toggle_play);
        }

        std::cout << "Music Player UI initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  SPACE  - Play/Pause\n";
        std::cout << "  LEFT   - Rewind\n";
        std::cout << "  RIGHT  - Forward\n";
        std::cout << "  ESC    - Quit\n\n";

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

    ~MusicPlayerDemo() {
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
    static constexpr int WIDTH = 400;
    static constexpr int HEIGHT = 700;
    static constexpr float SONG_DURATION = 225.0f;  // 3:45 in seconds

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Animation nodes
    flex::Node* album_cover_ = nullptr;
    std::vector<flex::Node*> visualizer_bars_;
    flex::Node* progress_fill_ = nullptr;
    flex::Node* progress_playhead_ = nullptr;
    flex::Node* current_time_text_ = nullptr;

    bool running_ = false;
    bool playing_ = true;
    float time_ = 0.0f;
    float song_time_ = 0.0f;

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    instance_->send_pointer_event(static_cast<float>(event.button.x), static_cast<float>(event.button.y), true);
                    break;
                case SDL_MOUSEBUTTONUP:
                    instance_->send_pointer_event(static_cast<float>(event.button.x), static_cast<float>(event.button.y), false);
                    break;
                case SDL_MOUSEMOTION:
                    instance_->send_pointer_event(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y), event.motion.state & SDL_BUTTON_LMASK);
                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running_ = false;
                            break;

                        case SDLK_SPACE:
                            playing_ = !playing_;
                            std::cout << (playing_ ? "▶ Playing" : "⏸ Paused") << "\n";
                            break;

                        case SDLK_LEFT:
                            song_time_ = std::max(0.0f, song_time_ - 10.0f);
                            std::cout << "Rewind to " << format_time(song_time_) << "\n";
                            break;

                        case SDLK_RIGHT:
                            song_time_ = std::min(SONG_DURATION, song_time_ + 10.0f);
                            std::cout << "Forward to " << format_time(song_time_) << "\n";
                            break;
                    }
                    break;
            }
        }
    }

    std::string format_time(float seconds) {
        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        std::ostringstream oss;
        oss << mins << ":" << std::setfill('0') << std::setw(2) << secs;
        return oss.str();
    }

    void update(float dt) {
        time_ += dt;

        // Update song time
        if (playing_) {
            song_time_ += dt;
            if (song_time_ >= SONG_DURATION) {
                song_time_ = 0.0f;  // Loop
            }
        }

        // Album rotation (slow spin)
        if (album_cover_ && playing_) {
            float rotation = fmod(time_ * 36.0f, 360.0f);  // 10 seconds per rotation
            album_cover_->set_rotation(rotation);
        }

        // Visualizer bars (animated based on beat)
        for (size_t i = 0; i < visualizer_bars_.size(); i++) {
            auto* bar = visualizer_bars_[i];
            if (!bar) continue;

            // Different frequency for each bar
            float freq = 2.0f + i * 0.3f;
            float phase = time_ * freq;
            float intensity = playing_ ? (std::sin(phase) * 0.5f + 0.5f) : 0.2f;

            // Base heights for each bar
            float base_heights[] = {40, 60, 80, 100, 120, 100, 80, 60, 40};
            float base = base_heights[i];
            float animated_height = base + intensity * 60.0f;

            if (auto* shape = dynamic_cast<flex::Shape*>(bar)) {
                shape->set_rect(12, animated_height);
                // Center bars vertically
                bar->set_y(-animated_height / 2);
            }
        }

        // Progress bar
        float progress_ratio = song_time_ / SONG_DURATION;
        float progress_width = progress_ratio * 300.0f;

        if (progress_fill_) {
            if (auto* shape = dynamic_cast<flex::Shape*>(progress_fill_)) {
                shape->set_rect(progress_width, 6);
            }
        }

        if (progress_playhead_) {
            float playhead_x = -150.0f + progress_width;
            progress_playhead_->set_x(playhead_x);
        }

        // Update time text
        if (current_time_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(current_time_text_)) {
                text->set_content(format_time(song_time_));
            }
        }

        // Advance animations
        instance_->advance(dt);
    }

    void render() {
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
    std::cout << "Music Player UI Demo - Flex Engine\n";
    std::cout << "===========================================\n\n";

    MusicPlayerDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
