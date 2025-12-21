/*
 * Component DSL Demo
 * Demonstrates using C++ registered components directly in .flex files
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

class ComponentDslDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Component DSL Demo - Declarative Component Usage",
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

        // Register all components BEFORE loading .flex file
        register_components();

        std::cout << "Loading Component DSL Demo...\\n";
        auto definition = flex::Definition::load_file("component_dsl_demo.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        std::cout << "Component DSL Demo initialized!\\n";
        std::cout << "Components used in .flex file:\\n";
        std::cout << "  - Slider (3 instances)\\n";
        std::cout << "  - ProgressBar (3 instances)\\n";
        std::cout << "  - LabeledSlider (3 instances, nested)\\n";
        std::cout << "  - SettingsRow (3 instances, nested)\\n\\n";
        std::cout << "Press ESC to quit\\n\\n";

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

    ~ComponentDslDemo() {
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
    static constexpr int WIDTH = 1200;
    static constexpr int HEIGHT = 800;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    bool running_ = false;

    void register_components() {
        std::cout << "Registering components for DSL use...\\n";

        // ============================================================
        // Base Widget: Slider
        // ============================================================
        auto slider_comp = flex::Component::create("Slider");
        slider_comp->add_prop("value", 0.5f);
        slider_comp->add_prop("min", 0.0f);
        slider_comp->add_prop("max", 1.0f);
        slider_comp->add_prop("width", 300.0f);
        slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
        slider_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto slider = flex::Group::create();

            float value = flex::get_prop_float(props, "value", 0.5f);
            float min = flex::get_prop_float(props, "min", 0.0f);
            float max = flex::get_prop_float(props, "max", 1.0f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            float normalized = (value - min) / (max - min);
            normalized = std::max(0.0f, std::min(1.0f, normalized));

            auto track = flex::Shape::create();
            track->set_rect(width, 8.0f);
            track->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            track->set_position(0.0f, 6.0f);
            slider->add_child(track);

            auto fill = flex::Shape::create();
            fill->set_rect(width * normalized, 8.0f);
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            fill->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            fill->set_position(0.0f, 6.0f);
            slider->add_child(fill);

            auto thumb = flex::Shape::create();
            thumb->set_circle(10.0f);
            thumb->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            thumb->set_position(width * normalized, 10.0f);
            slider->add_child(thumb);

            return slider;
        });
        flex::ComponentRegistry::instance().register_component(slider_comp);

        // ============================================================
        // Base Widget: ProgressBar
        // ============================================================
        auto progress_comp = flex::Component::create("ProgressBar");
        progress_comp->add_prop("progress", 0.5f);
        progress_comp->add_prop("width", 300.0f);
        progress_comp->add_prop("height", 20.0f);
        progress_comp->add_prop("color", uint32_t(0xFF198754));
        progress_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto progress = flex::Group::create();

            float value = flex::get_prop_float(props, "progress", 0.5f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            float height = flex::get_prop_float(props, "height", 20.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);

            value = std::max(0.0f, std::min(1.0f, value));

            auto bg = flex::Shape::create();
            bg->set_rect(width, height);
            bg->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            progress->add_child(bg);

            auto fill = flex::Shape::create();
            fill->set_rect(width * value, height);
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            fill->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            progress->add_child(fill);

            return progress;
        });
        flex::ComponentRegistry::instance().register_component(progress_comp);

        // ============================================================
        // Base Widget: Toggle
        // ============================================================
        auto toggle_comp = flex::Component::create("Toggle");
        toggle_comp->add_prop("on", false);
        toggle_comp->add_prop("width", 50.0f);
        toggle_comp->add_prop("height", 26.0f);
        toggle_comp->add_prop("color", uint32_t(0xFF198754));
        toggle_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto toggle = flex::Group::create();

            bool on = flex::get_prop_bool(props, "on");
            float width = flex::get_prop_float(props, "width", 50.0f);
            float height = flex::get_prop_float(props, "height", 26.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);

            auto track = flex::Shape::create();
            track->set_rect(width, height);

            if (on) {
                uint8_t a = (color >> 24) & 0xFF;
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                track->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            } else {
                track->set_fill(flex::Color(0.8f, 0.8f, 0.8f, 1.0f));
            }
            toggle->add_child(track);

            auto thumb = flex::Shape::create();
            thumb->set_circle(11.0f);
            thumb->set_fill(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));

            float thumb_x = on ? (width - 13.0f) : 13.0f;
            thumb->set_position(thumb_x, height / 2.0f);
            toggle->add_child(thumb);

            return toggle;
        });
        flex::ComponentRegistry::instance().register_component(toggle_comp);

        // ============================================================
        // Nested Component: LabeledSlider
        // ============================================================
        auto labeled_slider_comp = flex::Component::create("LabeledSlider");
        labeled_slider_comp->add_prop("label", std::string(""));
        labeled_slider_comp->add_prop("value", 0.5f);
        labeled_slider_comp->add_prop("width", 300.0f);
        labeled_slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
        labeled_slider_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();

            std::string label = flex::get_prop_string(props, "label");
            float value = flex::get_prop_float(props, "value");
            float width = flex::get_prop_float(props, "width");
            uint32_t color = flex::get_prop_color(props, "color");

            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            label_text->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            label_text->set_position(0.0f, 0.0f);
            group->add_child(label_text);

            auto slider = flex::create_component_instance("Slider", {
                {"value", value},
                {"width", width},
                {"color", color}
            });
            slider->set_position(0.0f, 25.0f);
            group->add_child(slider);

            auto value_text = flex::Text::create();
            std::ostringstream oss;
            oss << (int)(value * 100) << "%";
            value_text->set_content(oss.str());
            value_text->set_font_size(14.0f);
            value_text->set_color(flex::Color(0.05f, 0.43f, 0.99f, 1.0f));
            value_text->set_position(width + 20.0f, 30.0f);
            group->add_child(value_text);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(labeled_slider_comp);

        // ============================================================
        // Nested Component: SettingsRow
        // ============================================================
        auto settings_row_comp = flex::Component::create("SettingsRow");
        settings_row_comp->add_prop("label", std::string(""));
        settings_row_comp->add_prop("on", false);
        settings_row_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();

            std::string label = flex::get_prop_string(props, "label");
            bool on = flex::get_prop_bool(props, "on");

            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            label_text->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            label_text->set_position(0.0f, 10.0f);
            group->add_child(label_text);

            auto toggle = flex::create_component_instance("Toggle", {
                {"on", on},
                {"width", 50.0f},
                {"height", 26.0f}
            });
            toggle->set_position(250.0f, 0.0f);
            group->add_child(toggle);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(settings_row_comp);

        std::cout << "✅ 5 components registered (Slider, ProgressBar, Toggle, LabeledSlider, SettingsRow)\\n\\n";
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
                    }
                    break;
            }
        }
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
    std::cout << "===========================================\\n";
    std::cout << "Component DSL Demo\\n";
    std::cout << "===========================================\\n\\n";

    ComponentDslDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
