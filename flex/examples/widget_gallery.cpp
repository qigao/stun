/*
 * Widget Gallery - Standard UI Component Library
 * Demonstrates reusable UI widgets: Slider, ProgressBar, Checkbox, Radio, Toggle
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

class WidgetGallery {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Widget Gallery - Flex Component Library",
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

        std::cout << "Loading Widget Gallery...\n";
        auto definition = flex::Definition::load_file("widget_gallery.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Register widget components
        register_widgets();

        // Create widget instances
        create_widget_instances();

        std::cout << "Widget Gallery initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  MOUSE - Click widgets to interact\n";
        std::cout << "  DRAG  - Drag volume slider\n";
        std::cout << "  UP/DOWN - Adjust volume slider (keyboard)\n";
        std::cout << "  SPACE - Toggle mute (keyboard)\n";
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

    ~WidgetGallery() {
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

    // Interactive demo state
    float volume_ = 0.5f;  // 0-1
    bool muted_ = false;
    bool dragging_slider_ = false;
    bool volume_dirty_ = false;  // Flag to update widgets on next frame

    void register_widgets() {
        std::cout << "Registering widget components...\n";

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

            float value = flex::get_prop_float(props, "value", 0.5f);  // 0-1
            float min = flex::get_prop_float(props, "min", 0.0f);
            float max = flex::get_prop_float(props, "max", 1.0f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);  // Primary blue

            // Normalize value to 0-1 range
            float normalized = (value - min) / (max - min);
            normalized = std::max(0.0f, std::min(1.0f, normalized));

            // Track background
            auto track = flex::Shape::create();
            track->set_rect(width, 8.0f);
            track->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));  // #dee2e6
            track->set_position(0.0f, 6.0f);
            slider->add_child(track);

            // Fill (active part)
            auto fill = flex::Shape::create();
            fill->set_rect(width * normalized, 8.0f);
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            fill->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            fill->set_position(0.0f, 6.0f);
            slider->add_child(fill);

            // Thumb (handle)
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

            float value = flex::get_prop_float(props, "progress", 0.5f);  // 0-1
            float width = flex::get_prop_float(props, "width", 300.0f);
            float height = flex::get_prop_float(props, "height", 20.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);  // Success green

            value = std::max(0.0f, std::min(1.0f, value));

            // Background
            auto bg = flex::Shape::create();
            bg->set_rect(width, height);
            bg->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));  // #dee2e6
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
        // Checkbox Widget
        // ============================================================
        auto checkbox_comp = flex::Component::create("Checkbox");
        checkbox_comp->add_prop("checked", false, "Checked state");
        checkbox_comp->add_prop("label", std::string(""), "Label text");
        checkbox_comp->add_prop("color", uint32_t(0xFF0D6EFD), "Color (ARGB)");
        checkbox_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto checkbox = flex::Group::create();

            bool checked = flex::get_prop_bool(props, "checked", false);
            std::string label = flex::get_prop_string(props, "label", "");
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            // Box
            auto box = flex::Shape::create();
            box->set_rect(20.0f, 20.0f);

            if (checked) {
                uint8_t a = (color >> 24) & 0xFF;
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                box->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            } else {
                box->set_fill(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
                box->set_stroke(flex::Color(0.8f, 0.8f, 0.8f, 1.0f), 2.0f);
            }
            checkbox->add_child(box);

            // Checkmark (if checked)
            if (checked) {
                auto check = flex::Text::create();
                check->set_content("✓");
                check->set_font_size(16.0f);
                check->set_color(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
                check->set_position(10.0f, 16.0f);
                checkbox->add_child(check);
            }

            // Label
            if (!label.empty()) {
                auto text = flex::Text::create();
                text->set_content(label);
                text->set_font_size(14.0f);
                text->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
                text->set_position(30.0f, 15.0f);
                checkbox->add_child(text);
            }

            return checkbox;
        });
        flex::ComponentRegistry::instance().register_component(checkbox_comp);

        // ============================================================
        // Radio Widget
        // ============================================================
        auto radio_comp = flex::Component::create("Radio");
        radio_comp->add_prop("selected", false, "Selected state");
        radio_comp->add_prop("label", std::string(""), "Label text");
        radio_comp->add_prop("color", uint32_t(0xFF0D6EFD), "Color (ARGB)");
        radio_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto radio = flex::Group::create();

            bool selected = flex::get_prop_bool(props, "selected", false);
            std::string label = flex::get_prop_string(props, "label", "");
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            // Outer circle
            auto outer = flex::Shape::create();
            outer->set_circle(10.0f);
            outer->set_fill(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
            outer->set_stroke(flex::Color(0.8f, 0.8f, 0.8f, 1.0f), 2.0f);
            radio->add_child(outer);

            // Inner circle (if selected)
            if (selected) {
                auto inner = flex::Shape::create();
                inner->set_circle(6.0f);
                uint8_t a = (color >> 24) & 0xFF;
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                inner->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
                radio->add_child(inner);
            }

            // Label
            if (!label.empty()) {
                auto text = flex::Text::create();
                text->set_content(label);
                text->set_font_size(14.0f);
                text->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
                text->set_position(25.0f, 15.0f);
                radio->add_child(text);
            }

            return radio;
        });
        flex::ComponentRegistry::instance().register_component(radio_comp);

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
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);  // Success green

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

        std::cout << "✅ 5 widgets registered (Slider, ProgressBar, Checkbox, Radio, Toggle)\n\n";
    }

    void create_widget_instances() {
        auto* artboard = instance_->artboard();

        // ============================================================
        // Section 1: Sliders
        // ============================================================
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("sliderSection"))) {
            auto slider1 = flex::create_component_instance("Slider", {
                {"value", 0.3f},
                {"width", 300.0f},
                {"color", uint32_t(0xFF0D6EFD)}  // Primary blue
            });
            slider1->set_position(0.0f, 40.0f);
            section->add_child(slider1);

            auto slider2 = flex::create_component_instance("Slider", {
                {"value", 0.7f},
                {"width", 300.0f},
                {"color", uint32_t(0xFFDC3545)}  // Danger red
            });
            slider2->set_position(0.0f, 100.0f);
            section->add_child(slider2);

            auto slider3 = flex::create_component_instance("Slider", {
                {"value", 1.0f},
                {"width", 300.0f},
                {"color", uint32_t(0xFF198754)}  // Success green
            });
            slider3->set_position(0.0f, 160.0f);
            section->add_child(slider3);
        }

        // ============================================================
        // Section 2: Progress Bars
        // ============================================================
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("progressSection"))) {
            auto progress1 = flex::create_component_instance("ProgressBar", {
                {"progress", 0.25f},
                {"width", 300.0f},
                {"color", uint32_t(0xFF0D6EFD)}  // Primary blue
            });
            progress1->set_position(0.0f, 40.0f);
            section->add_child(progress1);

            auto progress2 = flex::create_component_instance("ProgressBar", {
                {"progress", 0.65f},
                {"width", 300.0f},
                {"color", uint32_t(0xFFFFC107)}  // Warning yellow
            });
            progress2->set_position(0.0f, 100.0f);
            section->add_child(progress2);

            auto progress3 = flex::create_component_instance("ProgressBar", {
                {"progress", 1.0f},
                {"width", 300.0f},
                {"color", uint32_t(0xFF198754)}  // Success green
            });
            progress3->set_position(0.0f, 160.0f);
            section->add_child(progress3);
        }

        // ============================================================
        // Section 3: Checkboxes
        // ============================================================
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("checkboxSection"))) {
            auto check1 = flex::create_component_instance("Checkbox", {
                {"checked", true},
                {"label", std::string("Option 1")}
            });
            check1->set_position(0.0f, 40.0f);
            section->add_child(check1);

            auto check2 = flex::create_component_instance("Checkbox", {
                {"checked", false},
                {"label", std::string("Option 2")}
            });
            check2->set_position(0.0f, 90.0f);
            section->add_child(check2);

            auto check3 = flex::create_component_instance("Checkbox", {
                {"checked", true},
                {"label", std::string("Option 3")}
            });
            check3->set_position(0.0f, 140.0f);
            section->add_child(check3);
        }

        // ============================================================
        // Section 4: Radio Buttons
        // ============================================================
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("radioSection"))) {
            auto radio1 = flex::create_component_instance("Radio", {
                {"selected", true},
                {"label", std::string("Choice A")}
            });
            radio1->set_position(0.0f, 40.0f);
            section->add_child(radio1);

            auto radio2 = flex::create_component_instance("Radio", {
                {"selected", false},
                {"label", std::string("Choice B")}
            });
            radio2->set_position(0.0f, 90.0f);
            section->add_child(radio2);

            auto radio3 = flex::create_component_instance("Radio", {
                {"selected", false},
                {"label", std::string("Choice C")}
            });
            radio3->set_position(0.0f, 140.0f);
            section->add_child(radio3);
        }

        // ============================================================
        // Section 5: Toggles
        // ============================================================
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("toggleSection"))) {
            auto toggle1 = flex::create_component_instance("Toggle", {
                {"on", true},
                {"width", 50.0f},
                {"height", 26.0f}
            });
            toggle1->set_position(0.0f, 40.0f);
            section->add_child(toggle1);

            auto toggle2 = flex::create_component_instance("Toggle", {
                {"on", false},
                {"width", 50.0f},
                {"height", 26.0f}
            });
            toggle2->set_position(0.0f, 100.0f);
            section->add_child(toggle2);

            auto toggle3 = flex::create_component_instance("Toggle", {
                {"on", true},
                {"width", 60.0f},
                {"height", 30.0f},
                {"color", uint32_t(0xFFDC3545)}  // Danger red
            });
            toggle3->set_position(0.0f, 160.0f);
            section->add_child(toggle3);
        }

        // ============================================================
        // Section 6: Interactive Demo
        // ============================================================
        create_demo_widgets();

        std::cout << "✅ Created widget instances\n\n";
    }

    void create_demo_widgets() {
        auto* artboard = instance_->artboard();
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("demoSection"))) {
            // Volume slider
            volume_slider_ = flex::create_component_instance("Slider", {
                {"value", volume_},
                {"width", 300.0f},
                {"color", uint32_t(0xFF0D6EFD)}
            });
            volume_slider_->set_position(100.0f, 35.0f);

            // Add drag interaction
            volume_slider_->on_pointer_down([this](flex::PointerEvent& e) {
                dragging_slider_ = true;

                // Calculate initial click position
                float slider_screen_x = 50.0f + 100.0f;
                float slider_width = 300.0f;
                float new_volume = (e.x - slider_screen_x) / slider_width;
                new_volume = std::max(0.0f, std::min(1.0f, new_volume));
                volume_ = new_volume;

                // Mark dirty for update on next frame (don't update in callback!)
                volume_dirty_ = true;

                e.stop_propagation();
            });

            section->add_child(volume_slider_);

            // Volume progress bar
            volume_progress_ = flex::create_component_instance("ProgressBar", {
                {"progress", volume_},
                {"width", 300.0f},
                {"height", 12.0f},
                {"color", uint32_t(0xFF0D6EFD)}
            });
            volume_progress_->set_position(100.0f, 80.0f);
            section->add_child(volume_progress_);

            // Mute toggle
            mute_toggle_ = flex::create_component_instance("Toggle", {
                {"on", muted_},
                {"width", 50.0f},
                {"height", 26.0f},
                {"color", uint32_t(0xFFDC3545)}
            });
            mute_toggle_->set_position(100.0f, 125.0f);

            // Add click interaction
            mute_toggle_->on_click([this]() {
                muted_ = !muted_;
                volume_dirty_ = true;  // Mark dirty instead of immediate update
            });

            section->add_child(mute_toggle_);

            // Update volume text
            if (auto* text = dynamic_cast<flex::Text*>(artboard->find("demoSection")->find("volumeValue"))) {
                std::ostringstream oss;
                oss << (int)(volume_ * 100) << "%";
                text->set_content(oss.str());
            }
        }
    }

    void update_demo_widgets() {
        auto* artboard = instance_->artboard();
        if (auto* section = dynamic_cast<flex::Group*>(artboard->find("demoSection"))) {
            // Remove old widgets
            section->remove_child(volume_slider_.get());
            section->remove_child(volume_progress_.get());
            section->remove_child(mute_toggle_.get());

            // Recreate with new values
            volume_slider_ = flex::create_component_instance("Slider", {
                {"value", volume_},
                {"width", 300.0f},
                {"color", uint32_t(0xFF0D6EFD)}
            });
            volume_slider_->set_position(100.0f, 35.0f);

            // Re-attach drag callback (重要！)
            volume_slider_->on_pointer_down([this](flex::PointerEvent& e) {
                dragging_slider_ = true;

                // Calculate click position
                float slider_screen_x = 50.0f + 100.0f;
                float slider_width = 300.0f;
                float new_volume = (e.x - slider_screen_x) / slider_width;
                new_volume = std::max(0.0f, std::min(1.0f, new_volume));
                volume_ = new_volume;

                // Mark dirty
                volume_dirty_ = true;

                e.stop_propagation();
            });

            section->add_child(volume_slider_);

            volume_progress_ = flex::create_component_instance("ProgressBar", {
                {"progress", volume_},
                {"width", 300.0f},
                {"height", 12.0f},
                {"color", uint32_t(0xFF0D6EFD)}
            });
            volume_progress_->set_position(100.0f, 80.0f);
            section->add_child(volume_progress_);

            mute_toggle_ = flex::create_component_instance("Toggle", {
                {"on", muted_},
                {"width", 50.0f},
                {"height", 26.0f},
                {"color", uint32_t(0xFFDC3545)}
            });
            mute_toggle_->set_position(100.0f, 125.0f);

            // Re-attach click callback (重要！)
            mute_toggle_->on_click([this]() {
                muted_ = !muted_;
                volume_dirty_ = true;
            });

            section->add_child(mute_toggle_);

            // Update volume text
            if (auto* text = dynamic_cast<flex::Text*>(artboard->find("demoSection")->find("volumeValue"))) {
                std::ostringstream oss;
                oss << (int)(volume_ * 100) << "%";
                text->set_content(oss.str());
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
                    if (dragging_slider_) {
                        dragging_slider_ = false;
                    }
                    break;

                case SDL_MOUSEMOTION:
                    instance_->send_pointer_event(
                        static_cast<float>(event.motion.x),
                        static_cast<float>(event.motion.y),
                        dragging_slider_
                    );
                    // Update volume while dragging slider
                    if (dragging_slider_) {
                        // Demo section at (50, 620), slider at (100, 35) within section
                        float slider_screen_x = 50.0f + 100.0f;  // 150
                        float slider_width = 300.0f;
                        float mouse_x = static_cast<float>(event.motion.x);

                        float new_volume = (mouse_x - slider_screen_x) / slider_width;
                        new_volume = std::max(0.0f, std::min(1.0f, new_volume));

                        if (std::abs(new_volume - volume_) > 0.01f) {
                            volume_ = new_volume;
                            volume_dirty_ = true;  // Mark dirty instead of immediate update
                        }
                    }
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running_ = false;
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        volume_ = std::min(1.0f, volume_ + 0.05f);
                        volume_dirty_ = true;
                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        volume_ = std::max(0.0f, volume_ - 0.05f);
                        volume_dirty_ = true;
                    } else if (event.key.keysym.sym == SDLK_SPACE) {
                        muted_ = !muted_;
                        volume_dirty_ = true;
                    }
                    break;
            }
        }
    }

    void update(float dt) {
        instance_->advance(dt);

        // Update widgets if volume changed (dirty flag pattern)
        if (volume_dirty_) {
            update_demo_widgets();
            volume_dirty_ = false;
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

    // Demo widget references
    std::shared_ptr<flex::Node> volume_slider_;
    std::shared_ptr<flex::Node> volume_progress_;
    std::shared_ptr<flex::Node> mute_toggle_;
};

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "Widget Gallery - Flex Component Library\n";
    std::cout << "===========================================\n\n";

    WidgetGallery demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
