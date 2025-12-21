/*
 * UI Components Gallery Demo
 * Showcase of interactive UI elements
 */

#include <iostream>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>

class UIComponentsDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "UI Components Gallery - Flex Engine Demo",
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

        std::cout << "Loading UI Components Gallery...\n";
        auto definition = flex::Definition::load_file("ui_components.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Find interactive components
        auto* artboard = instance_->artboard();

        // Buttons
        primary_button_ = artboard->find("primaryButton");
        secondary_button_ = artboard->find("secondaryButton");
        danger_button_ = artboard->find("dangerButton");
        outlined_button_ = artboard->find("outlinedButton");

        // Sliders
        slider1_fill_ = artboard->find("slider1")->find("fill");
        slider1_thumb_ = artboard->find("slider1")->find("thumb");
        slider1_label_ = artboard->find("slider1")->find("label");

        slider2_fill_ = artboard->find("slider2")->find("fill");
        slider2_thumb_ = artboard->find("slider2")->find("thumb");
        slider2_label_ = artboard->find("slider2")->find("label");

        // Checkboxes
        checkbox1_ = artboard->find("checkbox1");
        checkbox2_ = artboard->find("checkbox2");
        checkbox3_ = artboard->find("checkbox3");

        // Toggles
        toggle1_track_ = artboard->find("toggle1")->find("track");
        toggle1_thumb_ = artboard->find("toggle1")->find("thumb");
        toggle1_label_ = artboard->find("toggle1")->find("label");

        toggle2_track_ = artboard->find("toggle2")->find("track");
        toggle2_thumb_ = artboard->find("toggle2")->find("thumb");
        toggle2_label_ = artboard->find("toggle2")->find("label");

        // Progress bars
        progress1_fill_ = artboard->find("progress1")->find("fill");
        progress2_fill_ = artboard->find("progress2")->find("fill");

        std::cout << "UI Components Gallery initialized!\n";
        std::cout << "Interact with the UI elements!\n";
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
        }
    }

    ~UIComponentsDemo() {
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
    static constexpr int HEIGHT = 900;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // UI components
    flex::Node* primary_button_ = nullptr;
    flex::Node* secondary_button_ = nullptr;
    flex::Node* danger_button_ = nullptr;
    flex::Node* outlined_button_ = nullptr;

    flex::Node* slider1_fill_ = nullptr;
    flex::Node* slider1_thumb_ = nullptr;
    flex::Node* slider1_label_ = nullptr;

    flex::Node* slider2_fill_ = nullptr;
    flex::Node* slider2_thumb_ = nullptr;
    flex::Node* slider2_label_ = nullptr;

    flex::Node* checkbox1_ = nullptr;
    flex::Node* checkbox2_ = nullptr;
    flex::Node* checkbox3_ = nullptr;

    flex::Node* toggle1_track_ = nullptr;
    flex::Node* toggle1_thumb_ = nullptr;
    flex::Node* toggle1_label_ = nullptr;

    flex::Node* toggle2_track_ = nullptr;
    flex::Node* toggle2_thumb_ = nullptr;
    flex::Node* toggle2_label_ = nullptr;

    flex::Node* progress1_fill_ = nullptr;
    flex::Node* progress2_fill_ = nullptr;

    bool running_ = false;
    float time_ = 0.0f;

    // Component states
    float slider1_value_ = 0.75f;
    float slider2_value_ = 0.50f;
    bool toggle1_on_ = true;
    bool toggle2_on_ = false;

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

                case SDL_MOUSEBUTTONDOWN:
                    handle_click(event.button.x, event.button.y);
                    break;

                case SDL_MOUSEMOTION:
                    if (event.motion.state & SDL_BUTTON_LMASK) {
                        handle_drag(event.motion.x, event.motion.y);
                    }
                    break;
            }
        }
    }

    void handle_click(int x, int y) {
        // Check button clicks
        if (is_inside(x, y, 40, 130, 120, 40)) {
            std::cout << "Primary button clicked!\n";
            if (primary_button_) {
                primary_button_->set_scale(1.1f, 1.1f);
            }
        }
        else if (is_inside(x, y, 220, 130, 120, 40)) {
            std::cout << "Secondary button clicked!\n";
            if (secondary_button_) {
                secondary_button_->set_scale(1.1f, 1.1f);
            }
        }
        else if (is_inside(x, y, 400, 130, 120, 40)) {
            std::cout << "Danger button clicked!\n";
            if (danger_button_) {
                danger_button_->set_scale(1.1f, 1.1f);
            }
        }
        else if (is_inside(x, y, 580, 130, 120, 40)) {
            std::cout << "Outlined button clicked!\n";
            if (outlined_button_) {
                outlined_button_->set_scale(1.1f, 1.1f);
            }
        }

        // Check toggle clicks
        if (is_inside(x, y, 500, 627, 50, 26)) {
            toggle1_on_ = !toggle1_on_;
            update_toggle(toggle1_track_, toggle1_thumb_, toggle1_label_, toggle1_on_, "Dark Mode");
            std::cout << "Toggle 1: " << (toggle1_on_ ? "ON" : "OFF") << "\n";
        }
        else if (is_inside(x, y, 500, 687, 50, 26)) {
            toggle2_on_ = !toggle2_on_;
            update_toggle(toggle2_track_, toggle2_thumb_, toggle2_label_, toggle2_on_, "Auto-save");
            std::cout << "Toggle 2: " << (toggle2_on_ ? "ON" : "OFF") << "\n";
        }
    }

    void handle_drag(int x, int y) {
        // Slider 1
        if (y >= 280 && y <= 300) {
            if (x >= 100 && x <= 300) {
                slider1_value_ = (x - 100) / 200.0f;
                update_slider(slider1_fill_, slider1_thumb_, slider1_label_, slider1_value_, "Volume");
            }
        }

        // Slider 2
        if (y >= 280 && y <= 300) {
            if (x >= 450 && x <= 650) {
                slider2_value_ = (x - 450) / 200.0f;
                update_slider(slider2_fill_, slider2_thumb_, slider2_label_, slider2_value_, "Brightness");
            }
        }
    }

    bool is_inside(int px, int py, int x, int y, int w, int h) {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }

    void update_slider(flex::Node* fill, flex::Node* thumb, flex::Node* label, float value, const char* name) {
        float width = value * 200.0f;

        if (fill) {
            if (auto* shape = dynamic_cast<flex::Shape*>(fill)) {
                shape->set_rect(width, 8);
            }
        }

        if (thumb) {
            thumb->set_x(width);
        }

        if (label) {
            if (auto* text = dynamic_cast<flex::Text*>(label)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s: %.0f%%", name, value * 100.0f);
                text->set_content(buf);
            }
        }
    }

    void update_toggle(flex::Node* track, flex::Node* thumb, flex::Node* label, bool on, const char* name) {
        if (track) {
            if (auto* shape = dynamic_cast<flex::Shape*>(track)) {
                shape->set_fill(on ? flex::Color(0, 0.85f, 1, 1) : flex::Color(0.8f, 0.8f, 0.8f, 1));
            }
        }

        if (thumb) {
            thumb->set_x(on ? 30 : 20);
        }

        if (label) {
            if (auto* text = dynamic_cast<flex::Text*>(label)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s: %s", name, on ? "ON" : "OFF");
                text->set_content(buf);
            }
        }
    }

    void update(float dt) {
        time_ += dt;

        // Reset button scales
        if (primary_button_ && primary_button_->scale_x() > 1.0f) {
            primary_button_->set_scale(1.0f, 1.0f);
        }
        if (secondary_button_ && secondary_button_->scale_x() > 1.0f) {
            secondary_button_->set_scale(1.0f, 1.0f);
        }
        if (danger_button_ && danger_button_->scale_x() > 1.0f) {
            danger_button_->set_scale(1.0f, 1.0f);
        }
        if (outlined_button_ && outlined_button_->scale_x() > 1.0f) {
            outlined_button_->set_scale(1.0f, 1.0f);
        }

        // Animate progress bars
        float progress1 = std::fmod(time_ * 0.2f, 1.0f);
        float progress2 = std::fmod(time_ * 0.15f, 1.0f);

        if (progress1_fill_) {
            if (auto* shape = dynamic_cast<flex::Shape*>(progress1_fill_)) {
                shape->set_rect(progress1 * 300.0f, 12);
            }
        }

        if (progress2_fill_) {
            if (auto* shape = dynamic_cast<flex::Shape*>(progress2_fill_)) {
                shape->set_rect(progress2 * 300.0f, 12);
            }
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
    std::cout << "UI Components Gallery - Flex Engine\n";
    std::cout << "===========================================\n\n";

    UIComponentsDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
