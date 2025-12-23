/*
 * UI Components Gallery Demo
 * Showcase of buttons, sliders, toggles, checkboxes, radio buttons, progress bars
 * Interactive demonstration of common UI elements
 */

#include <iostream>
#include <cmath>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>
#include <flex/shape.h>
#include <flex/text.h>

class UIComponentsDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "UI Components Gallery",
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
        if (!instance_->artboard()) {
            std::cerr << "No artboard created!\n";
            return false;
        }

        std::cout << "✅ Instance created\n";
        std::cout << "  Artboard size (from flex): " << instance_->artboard()->width() << "x" << instance_->artboard()->height() << "\n";

        // Force artboard size to match window
        instance_->artboard()->set_size(WIDTH, HEIGHT);
        std::cout << "  Artboard size (after set): " << instance_->artboard()->width() << "x" << instance_->artboard()->height() << "\n";

        auto* root = instance_->artboard()->root();
        if (root) {
            std::cout << "  Root node children count: " << root->children().size() << "\n";

            // Debug: List all children
            for (size_t i = 0; i < root->children().size(); i++) {
                auto* child = root->children()[i].get();
                std::cout << "    Child " << i << ": type=" << child->type_name()
                          << " id='" << child->id() << "'"
                          << " visible=" << (child->visible() ? "yes" : "no") << "\n";

                // If it's a Shape, check geometry
                if (auto* shape = dynamic_cast<flex::Shape*>(child)) {
                    std::cout << "      Geometry type: " << (int)shape->geometry_type() << "\n";
                    std::cout << "      Has fill: " << (shape->has_fill() ? "yes" : "no") << "\n";
                    std::cout << "      Has stroke: " << (shape->has_stroke() ? "yes" : "no") << "\n";
                    std::cout << "      Position: (" << shape->x() << ", " << shape->y() << ")\n";

                    // For circles, check radius
                    if (shape->geometry_type() == flex::GeometryType::Circle) {
                        auto circle_geom = shape->circle();
                        std::cout << "      Circle radius: " << circle_geom.radius << "\n";
                    }

                    // For rects, check size
                    if (shape->geometry_type() == flex::GeometryType::Rect) {
                        auto rect_geom = shape->rect();
                        std::cout << "      Rect size: " << rect_geom.width << "x" << rect_geom.height << "\n";
                    }
                }
            }
        } else {
            std::cout << "  ERROR: Root node is null!\n";
        }

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        setup_interactive_elements();

        std::cout << "UI Components Gallery initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  Click buttons to interact\n";
        std::cout << "  T - Toggle dark mode\n";
        std::cout << "  A - Toggle auto-save\n";
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
            SDL_Delay(16);
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

    bool running_ = false;
    float time_ = 0.0f;

    // UI element references
    flex::Node* toggle1_ = nullptr;
    flex::Node* toggle2_ = nullptr;
    flex::Node* progress1_ = nullptr;
    flex::Node* progress2_ = nullptr;

    bool dark_mode_on_ = true;
    bool auto_save_on_ = false;

    void setup_interactive_elements() {
        auto* artboard = instance_->artboard();

        toggle1_ = artboard->find("toggle1");
        toggle2_ = artboard->find("toggle2");
        progress1_ = artboard->find("progress1");
        progress2_ = artboard->find("progress2");

        std::cout << "✅ Interactive elements initialized\n";
        std::cout << "  toggle1: " << (toggle1_ ? "found" : "NOT FOUND") << "\n";
        std::cout << "  toggle2: " << (toggle2_ ? "found" : "NOT FOUND") << "\n";
        std::cout << "  progress1: " << (progress1_ ? "found" : "NOT FOUND") << "\n";
        std::cout << "  progress2: " << (progress2_ ? "found" : "NOT FOUND") << "\n";

        // Test: Check if shapes exist
        if (progress1_) {
            auto* trackBar = progress1_->find("trackBar");
            auto* fill = progress1_->find("fill");
            std::cout << "  progress1/trackBar: " << (trackBar ? "found" : "NOT FOUND") << "\n";
            std::cout << "  progress1/fill: " << (fill ? "found" : "NOT FOUND") << "\n";
        }

        // Check basic shapes
        auto* bg = artboard->find("background");
        auto* primaryBtn = artboard->find("primaryButton");
        std::cout << "  background rect: " << (bg ? "found" : "NOT FOUND") << "\n";
        std::cout << "  primaryButton: " << (primaryBtn ? "found" : "NOT FOUND") << "\n";
    }

    void toggle_dark_mode() {
        dark_mode_on_ = !dark_mode_on_;

        if (toggle1_) {
            auto* trackBar = toggle1_->find("trackBar");
            auto* thumb = toggle1_->find("thumb");
            auto* label = toggle1_->find("label");

            if (trackBar) {
                auto* shape = dynamic_cast<flex::Shape*>(trackBar);
                if (shape) {
                    shape->set_fill(dark_mode_on_ ?
                        flex::Color(0, 217, 255) : flex::Color(204, 204, 204));
                }
            }

            if (thumb) {
                auto* circle = dynamic_cast<flex::Shape*>(thumb);
                if (circle) {
                    circle->set_position(dark_mode_on_ ? 30 : 20, 13);
                }
            }

            if (label) {
                auto* text = dynamic_cast<flex::Text*>(label);
                if (text) {
                    text->set_content(dark_mode_on_ ? "Dark Mode ON" : "Dark Mode OFF");
                }
            }
        }

        std::cout << "🌓 Dark Mode: " << (dark_mode_on_ ? "ON" : "OFF") << "\n";
    }

    void toggle_auto_save() {
        auto_save_on_ = !auto_save_on_;

        if (toggle2_) {
            auto* trackBar = toggle2_->find("trackBar");
            auto* thumb = toggle2_->find("thumb");
            auto* label = toggle2_->find("label");

            if (trackBar) {
                auto* shape = dynamic_cast<flex::Shape*>(trackBar);
                if (shape) {
                    shape->set_fill(auto_save_on_ ?
                        flex::Color(0, 217, 255) : flex::Color(204, 204, 204));
                }
            }

            if (thumb) {
                auto* circle = dynamic_cast<flex::Shape*>(thumb);
                if (circle) {
                    circle->set_position(auto_save_on_ ? 30 : 20, 13);
                }
            }

            if (label) {
                auto* text = dynamic_cast<flex::Text*>(label);
                if (text) {
                    text->set_content(auto_save_on_ ? "Auto save ON" : "Auto save OFF");
                }
            }
        }

        std::cout << "💾 Auto-save: " << (auto_save_on_ ? "ON" : "OFF") << "\n";
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
                    } else if (event.key.keysym.sym == SDLK_t) {
                        toggle_dark_mode();
                    } else if (event.key.keysym.sym == SDLK_a) {
                        toggle_auto_save();
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    int x = event.button.x;
                    int y = event.button.y;
                    handle_click(x, y);
                    break;
                }
            }
        }
    }

    void handle_click(int x, int y) {
        // Button zones (approximate)
        if (y >= 130 && y <= 170) {
            if (x >= 40 && x < 160) {
                std::cout << "✅ Primary Button Clicked\n";
            } else if (x >= 220 && x < 340) {
                std::cout << "✅ Secondary Button Clicked\n";
            } else if (x >= 400 && x < 520) {
                std::cout << "❌ Danger Button Clicked\n";
            } else if (x >= 580 && x < 700) {
                std::cout << "✅ Outlined Button Clicked\n";
            }
        }

        // Toggle zones (updated coordinates)
        if (y >= 510 && y <= 536) {
            if (x >= 100 && x <= 150) {
                toggle_dark_mode();
            }
        }

        if (y >= 570 && y <= 596) {
            if (x >= 100 && x <= 150) {
                toggle_auto_save();
            }
        }
    }

    void update(float dt) {
        time_ += dt;

        instance_->advance(dt);

        // Animate progress bars
        if (progress1_) {
            auto* fill = progress1_->find("fill");
            if (fill) {
                auto* rect = dynamic_cast<flex::Shape*>(fill);
                if (rect) {
                    float progress = 0.65f + 0.2f * std::sin(time_ * 0.5f);
                    progress = std::clamp(progress, 0.0f, 1.0f);
                    rect->set_rect(600 * progress, 12);
                }
            }

            auto* label = progress1_->find("label");
            if (label) {
                auto* text = dynamic_cast<flex::Text*>(label);
                if (text) {
                    float progress = 0.65f + 0.2f * std::sin(time_ * 0.5f);
                    progress = std::clamp(progress, 0.0f, 1.0f);
                    int percent = static_cast<int>(progress * 100);
                    text->set_content("Uploading " + std::to_string(percent));
                }
            }
        }

        if (progress2_) {
            auto* fill = progress2_->find("fill");
            if (fill) {
                auto* rect = dynamic_cast<flex::Shape*>(fill);
                if (rect) {
                    float progress = 0.40f + 0.3f * std::sin(time_ * 0.3f);
                    progress = std::clamp(progress, 0.0f, 1.0f);
                    rect->set_rect(600 * progress, 12);
                }
            }

            auto* label = progress2_->find("label");
            if (label) {
                auto* text = dynamic_cast<flex::Text*>(label);
                if (text) {
                    float progress = 0.40f + 0.3f * std::sin(time_ * 0.3f);
                    progress = std::clamp(progress, 0.0f, 1.0f);
                    int percent = static_cast<int>(progress * 100);
                    text->set_content("Processing " + std::to_string(percent));
                }
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
    std::cout << "UI Components Gallery\n";
    std::cout << "===========================================\n\n";

    UIComponentsDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
