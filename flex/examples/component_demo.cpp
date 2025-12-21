/*
 * Component System Demo
 * Demonstrates reusable UI components with props
 */

#include <iostream>
#include <sstream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>
#include <flex/component.h>
#include <flex/group.h>
#include <flex/shape.h>
#include <flex/text.h>

class ComponentDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Component System Demo",
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

        std::cout << "Loading Component Demo...\n";
        auto definition = flex::Definition::load_file("component_demo.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Register components
        register_components();

        // Create component instances
        create_instances();

        std::cout << "Component Demo initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  R - Recreate all components\n";
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
        }
    }

    ~ComponentDemo() {
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

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    bool running_ = false;
    int instance_count_ = 0;

    void register_components() {
        std::cout << "Registering components...\n";

        // Register Button component
        flex::register_component("Button", [](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto button = flex::Group::create();

            std::string label = flex::get_prop_string(props, "label", "Button");
            uint32_t color = flex::get_prop_color(props, "color", 0xFF00D9FF);
            float width = flex::get_prop_float(props, "width", 100.0f);
            float height = flex::get_prop_float(props, "height", 40.0f);

            // Background shape
            auto bg = flex::Shape::create();
            bg->set_rect(width, height);

            // Convert ARGB to Color (0xAARRGGBB -> Color)
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            bg->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));

            button->add_child(bg);

            // Label text
            auto text = flex::Text::create();
            text->set_content(label);
            text->set_font_size(14.0f);
            text->set_color(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));  // White
            text->set_position(width / 2.0f, height / 2.0f + 5.0f);
            button->add_child(text);

            return button;
        });

        // Register Card component
        flex::register_component("Card", [](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto card = flex::Group::create();

            std::string title = flex::get_prop_string(props, "title", "Card");
            std::string content = flex::get_prop_string(props, "content", "Content");
            float width = flex::get_prop_float(props, "width", 200.0f);
            float height = flex::get_prop_float(props, "height", 150.0f);

            // Card background
            auto bg = flex::Shape::create();
            bg->set_rect(width, height);
            bg->set_fill(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));  // White
            card->add_child(bg);

            // Title
            auto titleText = flex::Text::create();
            titleText->set_content(title);
            titleText->set_font_size(18.0f);
            titleText->set_color(flex::Color(0.17f, 0.17f, 0.33f, 1.0f));  // Dark
            titleText->set_position(20.0f, 35.0f);
            card->add_child(titleText);

            // Content
            auto contentText = flex::Text::create();
            contentText->set_content(content);
            contentText->set_font_size(14.0f);
            contentText->set_color(flex::Color(0.4f, 0.4f, 0.4f, 1.0f));  // Gray
            contentText->set_position(20.0f, 65.0f);
            card->add_child(contentText);

            return card;
        });

        // Register Badge component
        flex::register_component("Badge", [](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto badge = flex::Group::create();

            std::string label = flex::get_prop_string(props, "label", "Badge");
            uint32_t color = flex::get_prop_color(props, "color", 0xFFFF006E);

            // Badge background (circle-ish)
            auto bg = flex::Shape::create();
            bg->set_rect(60.0f, 25.0f);

            // Convert ARGB to Color (0xAARRGGBB -> Color)
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            bg->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));

            badge->add_child(bg);

            // Label
            auto text = flex::Text::create();
            text->set_content(label);
            text->set_font_size(12.0f);
            text->set_color(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));  // White
            text->set_position(30.0f, 17.0f);
            badge->add_child(text);

            return badge;
        });

        std::cout << "✅ 3 components registered (Button, Card, Badge)\n\n";
    }

    void create_instances() {
        auto* artboard = instance_->artboard();
        instance_count_ = 0;

        // Scene 1: Buttons
        if (auto* scene1 = dynamic_cast<flex::Group*>(artboard->find("scene1"))) {
            // Primary button
            auto btn1 = flex::create_component_instance("Button", {
                {"label", std::string("Primary")},
                {"color", uint32_t(0xFF00D9FF)},  // Cyan
                {"width", 100.0f},
                {"height", 40.0f}
            });
            if (btn1) {
                btn1->set_position(0.0f, 40.0f);
                scene1->add_child(btn1);
                instance_count_++;
            }

            // Secondary button
            auto btn2 = flex::create_component_instance("Button", {
                {"label", std::string("Secondary")},
                {"color", uint32_t(0xFF6C757D)},  // Gray
                {"width", 120.0f},
                {"height", 40.0f}
            });
            if (btn2) {
                btn2->set_position(120.0f, 40.0f);
                scene1->add_child(btn2);
                instance_count_++;
            }

            // Success button
            auto btn3 = flex::create_component_instance("Button", {
                {"label", std::string("Success")},
                {"color", uint32_t(0xFF00FF88)},  // Green
                {"width", 100.0f},
                {"height", 40.0f}
            });
            if (btn3) {
                btn3->set_position(260.0f, 40.0f);
                scene1->add_child(btn3);
                instance_count_++;
            }

            // Danger button
            auto btn4 = flex::create_component_instance("Button", {
                {"label", std::string("Danger")},
                {"color", uint32_t(0xFFFF006E)},  // Red
                {"width", 90.0f},
                {"height", 40.0f}
            });
            if (btn4) {
                btn4->set_position(380.0f, 40.0f);
                scene1->add_child(btn4);
                instance_count_++;
            }
        }

        // Scene 2: Cards
        if (auto* scene2 = dynamic_cast<flex::Group*>(artboard->find("scene2"))) {
            auto card1 = flex::create_component_instance("Card", {
                {"title", std::string("User Profile")},
                {"content", std::string("View your profile")},
                {"width", 200.0f},
                {"height", 150.0f}
            });
            if (card1) {
                card1->set_position(0.0f, 40.0f);
                scene2->add_child(card1);
                instance_count_++;
            }

            auto card2 = flex::create_component_instance("Card", {
                {"title", std::string("Settings")},
                {"content", std::string("Configure app")},
                {"width", 200.0f},
                {"height", 150.0f}
            });
            if (card2) {
                card2->set_position(240.0f, 40.0f);
                scene2->add_child(card2);
                instance_count_++;
            }

            auto card3 = flex::create_component_instance("Card", {
                {"title", std::string("Messages")},
                {"content", std::string("Read messages")},
                {"width", 200.0f},
                {"height", 150.0f}
            });
            if (card3) {
                card3->set_position(480.0f, 40.0f);
                scene2->add_child(card3);
                instance_count_++;
            }
        }

        // Scene 3: Badges
        if (auto* scene3 = dynamic_cast<flex::Group*>(artboard->find("scene3"))) {
            auto badge1 = flex::create_component_instance("Badge", {
                {"label", std::string("New")},
                {"color", uint32_t(0xFF00D9FF)}  // Cyan
            });
            if (badge1) {
                badge1->set_position(0.0f, 40.0f);
                scene3->add_child(badge1);
                instance_count_++;
            }

            auto badge2 = flex::create_component_instance("Badge", {
                {"label", std::string("Hot")},
                {"color", uint32_t(0xFFFF006E)}  // Red
            });
            if (badge2) {
                badge2->set_position(80.0f, 40.0f);
                scene3->add_child(badge2);
                instance_count_++;
            }

            auto badge3 = flex::create_component_instance("Badge", {
                {"label", std::string("Sale")},
                {"color", uint32_t(0xFFF39C12)}  // Orange
            });
            if (badge3) {
                badge3->set_position(160.0f, 40.0f);
                scene3->add_child(badge3);
                instance_count_++;
            }

            auto badge4 = flex::create_component_instance("Badge", {
                {"label", std::string("Beta")},
                {"color", uint32_t(0xFF9B59B6)}  // Purple
            });
            if (badge4) {
                badge4->set_position(240.0f, 40.0f);
                scene3->add_child(badge4);
                instance_count_++;
            }
        }

        update_stats();

        std::cout << "✅ Created " << instance_count_ << " component instances\n\n";
    }

    void update_stats() {
        auto* artboard = instance_->artboard();
        if (auto* stats = dynamic_cast<flex::Text*>(
                artboard->find("stats")->find("statsValue"))) {
            std::ostringstream oss;
            oss << "3 components registered, " << instance_count_ << " instances created";
            stats->set_content(oss.str());
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
                    } else if (event.key.keysym.sym == SDLK_r) {
                        recreate_instances();
                    }
                    break;
            }
        }
    }

    void recreate_instances() {
        std::cout << "🔄 Recreating component instances...\n";

        auto* artboard = instance_->artboard();

        // Clear existing instances
        if (auto* scene1 = dynamic_cast<flex::Group*>(artboard->find("scene1"))) {
            // Keep the label, remove buttons
            while (scene1->child_count() > 1) {
                scene1->remove_child_at(scene1->child_count() - 1);
            }
        }

        if (auto* scene2 = dynamic_cast<flex::Group*>(artboard->find("scene2"))) {
            while (scene2->child_count() > 1) {
                scene2->remove_child_at(scene2->child_count() - 1);
            }
        }

        if (auto* scene3 = dynamic_cast<flex::Group*>(artboard->find("scene3"))) {
            while (scene3->child_count() > 1) {
                scene3->remove_child_at(scene3->child_count() - 1);
            }
        }

        // Recreate
        create_instances();
    }

    void update(float dt) {
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
    std::cout << "Component System Demo\n";
    std::cout << "===========================================\n\n";

    ComponentDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
