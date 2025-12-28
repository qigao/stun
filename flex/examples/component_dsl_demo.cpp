/*
 * Component DSL Demo
 * Demonstrates using C++ registered components directly in .flex files
 */

#include <iostream>
#include <sstream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h" 

// ============================================================================
// Model - Application State
// ============================================================================

struct DemoModel {
    float slider1 = 0.3f;
    float slider2 = 0.7f;
    float slider3 = 1.0f;
    
    float progress1 = 0.25f;
    float progress2 = 0.65f;
    float progress3 = 1.0f;
    
    float brightness = 0.7f;
    float contrast = 0.5f;
    float saturation = 0.9f;
    
    bool notifications = true;
    bool autosave = false;
    bool darkmode = true;
};

// ============================================================================
// View - UI Presentation and Updates
// ============================================================================

class DemoView {
public:
    DemoView(flex::Instance::Ptr instance) : instance_(instance) {
        auto* scene = instance_->scene();
        
        // Find component nodes by ID (from .flex file)
        slider1 = scene->find("slider1");
        slider2 = scene->find("slider2");
        slider3 = scene->find("slider3");
        
        progress1 = scene->find("progress1");
        progress2 = scene->find("progress2");
        progress3 = scene->find("progress3");
        
        brightness = scene->find("brightness");
        contrast = scene->find("contrast");
        saturation = scene->find("saturation");
        
        notifications = scene->find("notifications");
        autosave = scene->find("autosave");
        darkmode = scene->find("darkmode");
    }

    void update(const DemoModel& model) {
        // Update basic Sliders
        update_slider_node(slider1, model.slider1);
        update_slider_node(slider2, model.slider2);
        update_slider_node(slider3, model.slider3);
        
        // Update Progress Bars
        update_progress_node(progress1, model.progress1);
        update_progress_node(progress2, model.progress2);
        update_progress_node(progress3, model.progress3);
        
        // Update Labeled Sliders (Nested)
        update_labeled_slider_node(brightness, model.brightness);
        update_labeled_slider_node(contrast, model.contrast);
        update_labeled_slider_node(saturation, model.saturation);
        
        // Update Settings Rows (Nested)
        update_settings_row_node(notifications, model.notifications);
        update_settings_row_node(autosave, model.autosave);
        update_settings_row_node(darkmode, model.darkmode);
    }

    // Accessors for Controller
    flex::Node* get_slider1() { return slider1; }
    flex::Node* get_slider2() { return slider2; }
    flex::Node* get_slider3() { return slider3; }
    flex::Node* get_brightness() { return brightness; }
    flex::Node* get_contrast() { return contrast; }
    flex::Node* get_saturation() { return saturation; }
    flex::Node* get_notifications() { return notifications; }
    flex::Node* get_autosave() { return autosave; }
    flex::Node* get_darkmode() { return darkmode; }

private:
    void update_slider_node(flex::Node* node, float value) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 3) return;
        
        // Slider builder order: [0] track, [1] fill, [2] thumb
        float width = 350.0f; // Default from DSL or registration
        float normalized = std::max(0.0f, std::min(1.0f, value));
        
        if (auto* fill = dynamic_cast<flex::Shape*>(group->child_at(1))) {
            fill->set_rect(width * normalized, 8.0f);
        }
        if (auto* thumb = dynamic_cast<flex::Shape*>(group->child_at(2))) {
            thumb->set_position(width * normalized, 10.0f);
        }
    }

    void update_progress_node(flex::Node* node, float value) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 2) return;
        
        // Progress builder order: [0] bg, [1] fill
        float width = 350.0f;
        float normalized = std::max(0.0f, std::min(1.0f, value));
        
        if (auto* fill = dynamic_cast<flex::Shape*>(group->child_at(1))) {
            fill->set_rect(width * normalized, 20.0f);
        }
    }

    void update_labeled_slider_node(flex::Node* node, float value) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 3) return;
        
        // LabeledSlider builder: [0] label, [1] Slider instance, [2] value text
        update_slider_node(group->child_at(1), value);
        
        if (auto* val_text = dynamic_cast<flex::Text*>(group->child_at(2))) {
            std::ostringstream oss;
            oss << (int)(value * 100) << "%";
            val_text->set_content(oss.str());
        }
    }

    void update_settings_row_node(flex::Node* node, bool on) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 2) return;
        
        // SettingsRow builder: [0] label, [1] Toggle instance
        update_toggle_node(group->child_at(1), on);
    }

    void update_toggle_node(flex::Node* node, bool on) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 2) return;
        
        // Toggle builder: [0] track, [1] thumb
        float width = 50.0f;
        if (auto* track = dynamic_cast<flex::Shape*>(group->child_at(0))) {
            if (on) track->set_fill(flex::Color(0.1f, 0.53f, 0.33f, 1.0f));
            else track->set_fill(flex::Color(0.8f, 0.8f, 0.8f, 1.0f));
        }
        if (auto* thumb = dynamic_cast<flex::Shape*>(group->child_at(1))) {
            float thumb_x = on ? (width - 13.0f) : 13.0f;
            thumb->set_position(thumb_x, 13.0f);
        }
    }

    flex::Instance::Ptr instance_;
    flex::Node *slider1 = nullptr, *slider2 = nullptr, *slider3 = nullptr;
    flex::Node *progress1 = nullptr, *progress2 = nullptr, *progress3 = nullptr;
    flex::Node *brightness = nullptr, *contrast = nullptr, *saturation = nullptr;
    flex::Node *notifications = nullptr, *autosave = nullptr, *darkmode = nullptr;
};

// ============================================================================
// Controller - Interaction Logic
// ============================================================================

class DemoController {
public:
    DemoController(DemoModel& model, DemoView& view) : model_(model), view_(view) {
        setup_interactions();
    }

    void update(float dt) {
        // Automatically progress progress bars
        model_.progress1 = std::fmod(model_.progress1 + dt * 0.1f, 1.0f);
        model_.progress2 = std::fmod(model_.progress2 + dt * 0.05f, 1.0f);
        model_.progress3 = std::fmod(model_.progress3 + dt * 0.2f, 1.0f);
        
        view_.update(model_);
    }

private:
    void setup_interactions() {
        // Toggle handlers
        auto setup_toggle = [this](flex::Node* node, bool& value, const std::string& name) {
            if (!node) return;
            auto* group = dynamic_cast<flex::Group*>(node);
            if (group && group->child_count() > 1) {
                group->child_at(1)->on_click([this, &value, name]() {
                    value = !value;
                    std::cout << "Toggle " << name << ": " << (value ? "ON" : "OFF") << "\n";
                    view_.update(model_);
                });
            }
        };

        setup_toggle(view_.get_notifications(), model_.notifications, "Notifications");
        setup_toggle(view_.get_autosave(), model_.autosave, "Autosave");
        setup_toggle(view_.get_darkmode(), model_.darkmode, "Dark Mode");

        // Slider handlers (Basic click-to-cycle for demo)
        auto setup_slider = [this](flex::Node* node, float& value, const std::string& name) {
            if (!node) return;
            node->on_click([this, &value, name]() {
                value = std::fmod(value + 0.1f, 1.05f);
                if (value > 1.0f) value = 0.0f;
                std::cout << "Slider " << name << " value changed to: " << (int)(value * 100) << "%\n";
                view_.update(model_);
            });
        };

        setup_slider(view_.get_slider1(), model_.slider1, "Slider 1");
        setup_slider(view_.get_slider2(), model_.slider2, "Slider 2");
        setup_slider(view_.get_slider3(), model_.slider3, "Slider 3");

        // Labeled Slider handlers
        auto setup_labeled_slider = [this](flex::Node* node, float& value, const std::string& name) {
            if (!node || !dynamic_cast<flex::Group*>(node)) return;
            auto* slider_child = dynamic_cast<flex::Group*>(node)->child_at(1);
            if (slider_child) {
                slider_child->on_click([this, &value, name]() {
                    value = std::fmod(value + 0.1f, 1.05f);
                    if (value > 1.0f) value = 0.0f;
                    std::cout << "Labeled Slider " << name << ": " << (int)(value * 100) << "%\n";
                    view_.update(model_);
                });
            }
        };

        setup_labeled_slider(view_.get_brightness(), model_.brightness, "Brightness");
        setup_labeled_slider(view_.get_contrast(), model_.contrast, "Contrast");
        setup_labeled_slider(view_.get_saturation(), model_.saturation, "Saturation");
    }

    DemoModel& model_;
    DemoView& view_;
};

// ============================================================================
// Application Shell
// ============================================================================

class ComponentDslDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Component DSL - MVC Demo",
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
        if (!instance_->scene()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Initialize MVC
        model_ = std::make_unique<DemoModel>();
        view_ = std::make_unique<DemoView>(instance_);
        controller_ = std::make_unique<DemoController>(*model_, *view_);

        // Sync initial state
        view_->update(*model_);

        std::cout << "Component DSL MVC Demo initialized!\\n";
        std::cout << "Interactive features:\\n";
        std::cout << "  - Click on Sliders to cycle values\\n";
        std::cout << "  - Click on Toggles to flip state\\n";
        std::cout << "  - Progress bars advance automatically\\n\\n";
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
            
            // MVC updates
            controller_->update(dt);
            instance_->advance(dt);
            
            render();
            SDL_Delay(16);
        }
    }

    ~ComponentDslDemo() {
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
    static constexpr int WIDTH = 1200;
    static constexpr int HEIGHT = 800;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    std::unique_ptr<DemoModel> model_;
    std::unique_ptr<DemoView> view_;
    std::unique_ptr<DemoController> controller_;

    bool running_ = false;

    void register_components() {
        // [Component registration logic remains same as before but encapsulated]
        
        // Slider
        auto slider_comp = flex::Component::create("Slider");
        slider_comp->add_prop("value", 0.5f);
        slider_comp->add_prop("width", 300.0f);
        slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
        slider_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto slider = flex::Group::create();
            float value = flex::get_prop_float(props, "value", 0.5f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            auto track = flex::Shape::create();
            track->set_rect(width, 8.0f);
            track->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            track->set_position(0.0f, 6.0f);
            slider->add_child(track);

            auto fill = flex::Shape::create();
            fill->set_rect(width * value, 8.0f);
            uint8_t a = (color >> 24) & 0xFF, r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
            fill->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            fill->set_position(0.0f, 6.0f);
            slider->add_child(fill);

            auto thumb = flex::Shape::create();
            thumb->set_circle(10.0f);
            thumb->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            thumb->set_position(width * value, 10.0f);
            slider->add_child(thumb);

            return slider;
        });
        flex::ComponentRegistry::instance().register_component(slider_comp);

        // ProgressBar
        auto progress_comp = flex::Component::create("ProgressBar");
        progress_comp->add_prop("progress", 0.5f);
        progress_comp->add_prop("width", 300.0f);
        progress_comp->add_prop("height", 20.0f);  // ADDED: height property
        progress_comp->add_prop("color", uint32_t(0xFF198754));
        progress_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto progress = flex::Group::create();
            float value = flex::get_prop_float(props, "progress", 0.5f);
            float width = flex::get_prop_float(props, "width", 300.0f);
            float height = flex::get_prop_float(props, "height", 20.0f);  // ADDED: read height
            uint32_t color = flex::get_prop_color(props, "color", 0xFF198754);

            auto bg = flex::Shape::create();
            bg->set_rect(width, height);  // FIXED: use height instead of hardcoded 20.0f
            bg->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            progress->add_child(bg);

            auto fill = flex::Shape::create();
            fill->set_rect(width * value, height);  // FIXED: use height instead of hardcoded 20.0f
            uint8_t a = (color >> 24) & 0xFF, r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
            fill->set_fill(flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
            progress->add_child(fill);

            return progress;
        });
        flex::ComponentRegistry::instance().register_component(progress_comp);

        // Toggle
        auto toggle_comp = flex::Component::create("Toggle");
        toggle_comp->add_prop("on", false);
        toggle_comp->add_prop("width", 50.0f);
        toggle_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto toggle = flex::Group::create();
            bool on = flex::get_prop_bool(props, "on");
            float width = flex::get_prop_float(props, "width", 50.0f);

            auto track = flex::Shape::create();
            track->set_rect(width, 26.0f);
            track->set_fill(on ? flex::Color(0.1f, 0.53f, 0.33f, 1.0f) : flex::Color(0.8f, 0.8f, 0.8f, 1.0f));
            toggle->add_child(track);

            auto thumb = flex::Shape::create();
            thumb->set_circle(11.0f);
            thumb->set_fill(flex::Color(1, 1, 1, 1));
            thumb->set_position(on ? (width - 13.0f) : 13.0f, 13.0f);
            toggle->add_child(thumb);

            return toggle;
        });
        flex::ComponentRegistry::instance().register_component(toggle_comp);

        // LabeledSlider
        auto labeled_slider_comp = flex::Component::create("LabeledSlider");
        labeled_slider_comp->add_prop("label", std::string(""));
        labeled_slider_comp->add_prop("value", 0.5f);
        labeled_slider_comp->add_prop("width", 400.0f);  // ADDED: width property
        labeled_slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));  // ADDED: color property
        labeled_slider_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            auto group = flex::Group::create();
            std::string label = flex::get_prop_string(props, "label");
            float value = flex::get_prop_float(props, "value");
            float width = flex::get_prop_float(props, "width", 400.0f);  // ADDED: read width
            uint32_t color = flex::get_prop_color(props, "color", 0xFF0D6EFD);  // ADDED: read color

            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            group->add_child(label_text);

            // FIXED: pass width and color to nested Slider
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
            value_text->set_position(width + 20.0f, 30.0f);  // FIXED: position relative to width
            group->add_child(value_text);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(labeled_slider_comp);

        // SettingsRow
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
            label_text->set_position(0.0f, 10.0f);
            group->add_child(label_text);

            auto toggle = flex::create_component_instance("Toggle", {{"on", on}});
            toggle->set_position(250.0f, 0.0f);
            group->add_child(toggle);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(settings_row_comp);
    }

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: running_ = false; break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) running_ = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    instance_->send_pointer_event((float)event.button.x, (float)event.button.y, true);
                    break;
                case SDL_MOUSEBUTTONUP:
                    instance_->send_pointer_event((float)event.button.x, (float)event.button.y, false);
                    break;
                case SDL_MOUSEMOTION:
                    instance_->send_pointer_event((float)event.motion.x, (float)event.motion.y, (event.motion.state & SDL_BUTTON_LMASK) != 0);
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
    ComponentDslDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}

