/*
 * Nested Components Demo
 * Demonstrates component composition - building complex UIs from simple widgets
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

class NestedComponentsDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Nested Components Demo - Component Composition",
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

        std::cout << "Loading Nested Components Demo...\n";
        auto definition = flex::Definition::load_file("nested_components.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Register base widgets
        register_base_widgets();

        // Register nested components
        register_nested_components();

        // Create component instances
        create_component_instances();

        std::cout << "Nested Components Demo initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  MOUSE - Click widgets to interact\n";
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

    ~NestedComponentsDemo() {
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

    void register_base_widgets() {
        std::cout << "Registering base widgets...\n";

        // ============================================================
        // Slider Widget
        // ============================================================
        auto slider_comp = flex::Component::create("Slider");
        slider_comp->add_prop("value", 0.5f, "Current value 0-1");
        slider_comp->add_prop("min", 0.0f, "Minimum value");
        slider_comp->add_prop("max", 1.0f, "Maximum value");
        slider_comp->add_prop("width", 300.0f, "Slider width");
        slider_comp->add_prop("color", uint32_t(0xFF0D6EFD), "Color (ARGB)");
        slider_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto slider = flex::Group::create();

            float value = flex::get_prop_float(props, "value", 0.5f);
            float min = flex::get_prop_float(props, "min", 0.0f);
            float max = flex::get_prop_float(props, "max", 1.0f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            float normalized = (value - min) / (max - min);
            normalized = std::max(0.0f, std::min(1.0f, normalized));

            // Track
            auto track = flex::Shape::create();
            track->set_rect(width, 8.0f);
            track->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            track->set_position(0.0f, 6.0f);
            slider->add_child(track);

            // Fill
            auto fill = flex::Shape::create();
            fill->set_rect(width * normalized, 8.0f);
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            fill->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            fill->set_position(0.0f, 6.0f);
            slider->add_child(fill);

            // Thumb
            auto thumb = flex::Shape::create();
            thumb->set_circle(10.0f);
            thumb->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            thumb->set_position(width * normalized, 10.0f);
            slider->add_child(thumb);

            return slider;
        });
        flex::ComponentRegistry::instance().register_component(slider_comp);

        // ============================================================
        // ProgressBar Widget
        // ============================================================
        auto progress_comp = flex::Component::create("ProgressBar");
        progress_comp->add_prop("progress", 0.5f, "Progress 0-1");
        progress_comp->add_prop("width", 300.0f, "Bar width");
        progress_comp->add_prop("height", 20.0f, "Bar height");
        progress_comp->add_prop("color", uint32_t(0xFF198754), "Color (ARGB)");
        progress_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto progress = flex::Group::create();

            float value = flex::get_prop_float(props, "progress", 0.5f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            float height = flex::get_prop_float(props, "height", 20.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);

            value = std::max(0.0f, std::min(1.0f, value));

            // Background
            auto bg = flex::Shape::create();
            bg->set_rect(width, height);
            bg->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            progress->add_child(bg);

            // Fill
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
        // Toggle Widget
        // ============================================================
        auto toggle_comp = flex::Component::create("Toggle");
        toggle_comp->add_prop("on", false, "Toggle state");
        toggle_comp->add_prop("width", 50.0f, "Toggle width");
        toggle_comp->add_prop("height", 26.0f, "Toggle height");
        toggle_comp->add_prop("color", uint32_t(0xFF198754), "Color (ARGB)");
        toggle_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto toggle = flex::Group::create();

            bool on = flex::get_prop_bool(props, "on", false);
            float width = flex::get_prop_float(props, "width", 50.0f);
            float height = flex::get_prop_float(props, "height", 26.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);

            // Track
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

            // Thumb
            auto thumb = flex::Shape::create();
            thumb->set_circle(11.0f);
            thumb->set_fill(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));

            float thumb_x = on ? (width - 13.0f) : 13.0f;
            thumb->set_position(thumb_x, height / 2.0f);
            toggle->add_child(thumb);

            return toggle;
        });
        flex::ComponentRegistry::instance().register_component(toggle_comp);

        std::cout << "✅ 3 base widgets registered (Slider, ProgressBar, Toggle)\n\n";
    }

    void register_nested_components() {
        std::cout << "Registering nested components...\n";

        // ============================================================
        // LabeledSlider - Label + Slider + Value Display
        // ============================================================
        auto labeled_slider_comp = flex::Component::create("LabeledSlider");
        labeled_slider_comp->add_prop("label", std::string(""), "Label text");
        labeled_slider_comp->add_prop("value", 0.5f, "Current value");
        labeled_slider_comp->add_prop("width", 300.0f, "Slider width");
        labeled_slider_comp->add_prop("color", uint32_t(0xFF0D6EFD), "Color");
        labeled_slider_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();

            std::string label = flex::get_prop_string(props, "label");
            float value = flex::get_prop_float(props, "value");
            float width = flex::get_prop_float(props, "width");
            uint32_t color = flex::get_prop_color(props, "color");

            // Label
            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            label_text->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            label_text->set_position(0.0f, 0.0f);
            group->add_child(label_text);

            // Slider (nested component!)
            auto slider = flex::create_component_instance("Slider", {
                {"value", value},
                {"width", width},
                {"color", color}
            });
            slider->set_position(0.0f, 25.0f);
            group->add_child(slider);

            // Value display
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
        // SettingsRow - Label + Toggle
        // ============================================================
        auto settings_row_comp = flex::Component::create("SettingsRow");
        settings_row_comp->add_prop("label", std::string(""), "Setting label");
        settings_row_comp->add_prop("on", false, "Toggle state");
        settings_row_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();

            std::string label = flex::get_prop_string(props, "label");
            bool on = flex::get_prop_bool(props, "on");

            // Label
            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            label_text->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            label_text->set_position(0.0f, 10.0f);
            group->add_child(label_text);

            // Toggle (nested component!)
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

        // ============================================================
        // VolumeControl - Slider + ProgressBar + Toggle (3 nested!)
        // ============================================================
        auto volume_ctrl_comp = flex::Component::create("VolumeControl");
        volume_ctrl_comp->add_prop("volume", 0.5f, "Volume level");
        volume_ctrl_comp->add_prop("muted", false, "Mute state");
        volume_ctrl_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();

            float volume = flex::get_prop_float(props, "volume");
            bool muted = flex::get_prop_bool(props, "muted");

            // Title
            auto title = flex::Text::create();
            title->set_content("Volume Control");
            title->set_font_size(16.0f);
            title->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            title->set_position(0.0f, 0.0f);
            group->add_child(title);

            // Slider (nested!)
            auto slider = flex::create_component_instance("Slider", {
                {"value", volume},
                {"width", 400.0f}
            });
            slider->set_position(0.0f, 30.0f);
            group->add_child(slider);

            // ProgressBar (nested!)
            auto progress = flex::create_component_instance("ProgressBar", {
                {"progress", volume},
                {"width", 400.0f},
                {"height", 12.0f}
            });
            progress->set_position(0.0f, 65.0f);
            group->add_child(progress);

            // Mute toggle (nested!)
            auto mute_label = flex::Text::create();
            mute_label->set_content("Mute:");
            mute_label->set_font_size(14.0f);
            mute_label->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            mute_label->set_position(0.0f, 100.0f);
            group->add_child(mute_label);

            auto toggle = flex::create_component_instance("Toggle", {
                {"on", muted},
                {"width", 50.0f},
                {"height", 26.0f},
                {"color", uint32_t(0xFFDC3545)}
            });
            toggle->set_position(60.0f, 95.0f);
            group->add_child(toggle);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(volume_ctrl_comp);

        // ============================================================
        // SettingsPanel - Multiple SettingsRow (deeply nested!)
        // ============================================================
        auto settings_panel_comp = flex::Component::create("SettingsPanel");
        settings_panel_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();

            // Title
            auto title = flex::Text::create();
            title->set_content("Settings Panel");
            title->set_font_size(16.0f);
            title->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
            title->set_position(0.0f, 0.0f);
            group->add_child(title);

            // Row 1 (nested SettingsRow!)
            auto row1 = flex::create_component_instance("SettingsRow", {
                {"label", std::string("Notifications")},
                {"on", true}
            });
            row1->set_position(0.0f, 30.0f);
            group->add_child(row1);

            // Row 2
            auto row2 = flex::create_component_instance("SettingsRow", {
                {"label", std::string("Auto-save")},
                {"on", true}
            });
            row2->set_position(0.0f, 70.0f);
            group->add_child(row2);

            // Row 3
            auto row3 = flex::create_component_instance("SettingsRow", {
                {"label", std::string("Dark Mode")},
                {"on", false}
            });
            row3->set_position(0.0f, 110.0f);
            group->add_child(row3);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(settings_panel_comp);

        std::cout << "✅ 4 nested components registered\n";
        std::cout << "   - LabeledSlider (Label + Slider + Value)\n";
        std::cout << "   - SettingsRow (Label + Toggle)\n";
        std::cout << "   - VolumeControl (Slider + ProgressBar + Toggle)\n";
        std::cout << "   - SettingsPanel (3x SettingsRow)\n\n";
    }

    void create_component_instances() {
        auto* artboard = instance_->artboard();

        // Section 1: LabeledSlider
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("section1"))) {
            auto slider1 = flex::create_component_instance("LabeledSlider", {
                {"label", std::string("Brightness")},
                {"value", 0.7f},
                {"width", 350.0f},
                {"color", uint32_t(0xFF0D6EFD)}
            });
            slider1->set_position(0.0f, 40.0f);
            section->add_child(slider1);

            auto slider2 = flex::create_component_instance("LabeledSlider", {
                {"label", std::string("Contrast")},
                {"value", 0.5f},
                {"width", 350.0f},
                {"color", uint32_t(0xFF198754)}
            });
            slider2->set_position(0.0f, 120.0f);
            section->add_child(slider2);

            auto slider3 = flex::create_component_instance("LabeledSlider", {
                {"label", std::string("Saturation")},
                {"value", 0.9f},
                {"width", 350.0f},
                {"color", uint32_t(0xFFFFC107)}
            });
            slider3->set_position(0.0f, 200.0f);
            section->add_child(slider3);
        }

        // Section 2: SettingsRow
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("section2"))) {
            auto row1 = flex::create_component_instance("SettingsRow", {
                {"label", std::string("Enable Notifications")},
                {"on", true}
            });
            row1->set_position(0.0f, 40.0f);
            section->add_child(row1);

            auto row2 = flex::create_component_instance("SettingsRow", {
                {"label", std::string("Auto-update")},
                {"on", false}
            });
            row2->set_position(0.0f, 100.0f);
            section->add_child(row2);

            auto row3 = flex::create_component_instance("SettingsRow", {
                {"label", std::string("Save on Exit")},
                {"on", true}
            });
            row3->set_position(0.0f, 160.0f);
            section->add_child(row3);
        }

        // Section 3: VolumeControl
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("section3"))) {
            auto volume = flex::create_component_instance("VolumeControl", {
                {"volume", 0.65f},
                {"muted", false}
            });
            volume->set_position(0.0f, 40.0f);
            section->add_child(volume);
        }

        // Section 4: SettingsPanel
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("section4"))) {
            auto panel = flex::create_component_instance("SettingsPanel", {});
            panel->set_position(0.0f, 40.0f);
            section->add_child(panel);
        }

        std::cout << "✅ Created all component instances\n\n";
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

                case SDL_MOUSEBUTTONDOWN:
                    instance_->send_pointer_event(
                        static_cast<float>(event.button.x),
                        static_cast<float>(event.button.y),
                        true
                    );
                    break;

                case SDL_MOUSEBUTTONUP:
                    instance_->send_pointer_event(
                        static_cast<float>(event.button.x),
                        static_cast<float>(event.button.y),
                        false
                    );
                    break;

                case SDL_MOUSEMOTION:
                    instance_->send_pointer_event(
                        static_cast<float>(event.motion.x),
                        static_cast<float>(event.motion.y),
                        false
                    );
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
    std::cout << "===========================================\n";
    std::cout << "Nested Components Demo\n";
    std::cout << "===========================================\n\n";

    NestedComponentsDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
