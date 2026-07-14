/*
 * Nested Components Demo - MVC Refactor
 * Demonstrates component composition - building complex UIs from simple widgets
 * Separates data (Model), presentation (View), and logic (Controller).
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "backends/thorvg/init.h" 

// ============================================================================
// Model - Application State
// ============================================================================

struct NestedModel {
    // Section 1: Basic
    float basic_slider = 0.65f;
    bool basic_toggle = true;
    float basic_progress = 0.75f;
    
    // Section 2: Labeled Sliders
    float volume = 0.65f;
    float brightness = 0.80f;
    float contrast = 0.50f;
    
    // Section 3: Settings Rows
    bool dark_mode = false;
    bool notifications = true;
    bool auto_save = true;
    
    // Section 4: Volume Control
    float master_volume = 0.75f;
    bool master_muted = false;
    
    // Section 5: Settings Panel
    bool hdr_enabled = true;
    bool vsync_enabled = false;
    bool show_fps = true;
    bool full_screen = false;
};

// ============================================================================
// View - UI Presentation and Updates
// ============================================================================

class NestedView {
public:
    NestedView(flex::Instance::SharedPtr instance) : instance_(instance) {
        auto* scene = instance_->scene();
        
        // Find nodes by ID
        basic_slider = scene->find("basicSlider");
        basic_toggle = scene->find("basicToggle");
        basic_progress = scene->find("basicProgress");
        
        volume = scene->find("volume");
        brightness = scene->find("brightness");
        contrast = scene->find("contrast");
        
        dark_mode = scene->find("darkMode");
        notifications = scene->find("notifications");
        auto_save = scene->find("autoSave");
        
        master_ctrl = scene->find("audioMaster");
        
        s1 = scene->find("s1");
        s2 = scene->find("s2");
        s3 = scene->find("s3");
        s4 = scene->find("s4");
    }

    void update(const NestedModel& model) {
        // Section 1
        update_slider_node(basic_slider, model.basic_slider);
        update_toggle_node(basic_toggle, model.basic_toggle);
        update_progress_node(basic_progress, model.basic_progress);
        
        // Section 2
        update_labeled_slider_node(volume, model.volume);
        update_labeled_slider_node(brightness, model.brightness);
        update_labeled_slider_node(contrast, model.contrast);
        
        // Section 3
        update_settings_row_node(dark_mode, model.dark_mode);
        update_settings_row_node(notifications, model.notifications);
        update_settings_row_node(auto_save, model.auto_save);
        
        // Section 4
        update_volume_ctrl_node(master_ctrl, model.master_volume, model.master_muted);
        
        // Section 5
        update_settings_row_node(s1, model.hdr_enabled);
        update_settings_row_node(s2, model.vsync_enabled);
        update_settings_row_node(s3, model.show_fps);
        update_settings_row_node(s4, model.full_screen);
    }

    // Accessors for Controller
    flex::Node* get_basic_slider() { return basic_slider; }
    flex::Node* get_basic_toggle() { return basic_toggle; }
    flex::Node* get_volume() { return volume; }
    flex::Node* get_brightness() { return brightness; }
    flex::Node* get_contrast() { return contrast; }
    flex::Node* get_dark_mode() { return dark_mode; }
    flex::Node* get_notifications() { return notifications; }
    flex::Node* get_auto_save() { return auto_save; }
    flex::Node* get_master_ctrl() { return master_ctrl; }
    flex::Node* get_s1() { return s1; }
    flex::Node* get_s2() { return s2; }
    flex::Node* get_s3() { return s3; }
    flex::Node* get_s4() { return s4; }

private:
    void update_slider_node(flex::Node* node, float value) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 3) return;
        
        float width = 400.0f; 
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
        
        float width = 400.0f;
        float normalized = std::max(0.0f, std::min(1.0f, value));
        float height = 20.0f;
        
        if (auto* fill = dynamic_cast<flex::Shape*>(group->child_at(1))) {
            fill->set_rect(width * normalized, height);
        }
    }

    void update_toggle_node(flex::Node* node, bool on) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 2) return;
        
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

    void update_labeled_slider_node(flex::Node* node, float value) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 3) return;
        
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
        
        update_toggle_node(group->child_at(1), on);
    }

    void update_volume_ctrl_node(flex::Node* node, float vol, bool muted) {
        auto* group = dynamic_cast<flex::Group*>(node);
        if (!group || group->child_count() < 5) return;
        
        // VolumeControl builder: [0] title, [1] Slider, [2] ProgressBar, [3] mute_label, [4] Toggle
        update_slider_node(group->child_at(1), vol);
        update_progress_node(group->child_at(2), vol);
        update_toggle_node(group->child_at(4), muted);
        
        if (auto* prog_group = dynamic_cast<flex::Group*>(group->child_at(2))) {
            if (prog_group->child_count() > 1) {
                if (auto* prog_fill = dynamic_cast<flex::Shape*>(prog_group->child_at(1))) {
                    if (muted) prog_fill->set_fill(flex::Color(0.5f, 0.5f, 0.5f, 1.0f));
                    else prog_fill->set_fill(flex::Color(0.1f, 0.53f, 0.33f, 1.0f));
                }
            }
        }
    }

    flex::Instance::SharedPtr instance_;
    flex::Node *basic_slider, *basic_toggle, *basic_progress;
    flex::Node *volume, *brightness, *contrast;
    flex::Node *dark_mode, *notifications, *auto_save;
    flex::Node *master_ctrl;
    flex::Node *s1, *s2, *s3, *s4;
};

// ============================================================================
// Controller - Interaction Logic
// ============================================================================

class NestedController {
public:
    NestedController(NestedModel& model, NestedView& view) : model_(model), view_(view) {
        setup_interactions();
    }

    void update(float dt) {
        // Auto-update basic progress bar
        model_.basic_progress = std::fmod(model_.basic_progress + dt * 0.1f, 1.05f);
        if (model_.basic_progress > 1.0f) model_.basic_progress = 0.0f;
        
        view_.update(model_);
    }

private:
    void setup_interactions() {
        // Basic Slider
        if (auto* n = view_.get_basic_slider()) {
            n->on_click([this]() { 
                model_.basic_slider = std::fmod(model_.basic_slider + 0.1f, 1.05f);
                if (model_.basic_slider > 1.0f) model_.basic_slider = 0.0f;
                view_.update(model_);
            });
        }
        
        // Basic Toggle
        if (auto* n = view_.get_basic_toggle()) {
            n->on_click([this]() { model_.basic_toggle = !model_.basic_toggle; view_.update(model_); });
        }

        // Helper for Slider interaction
        auto setup_slider = [this](flex::Node* node, float& val) {
            if (!node) return;
            auto* group = dynamic_cast<flex::Group*>(node);
            if (group && group->child_count() > 1) {
                group->child_at(1)->on_click([this, &val]() {
                    val = std::fmod(val + 0.1f, 1.05f);
                    if (val > 1.0f) val = 0.0f;
                    view_.update(model_);
                });
            }
        };

        // Helper for Toggle interaction
        auto setup_toggle = [this](flex::Node* node, bool& val) {
            if (!node) return;
            auto* group = dynamic_cast<flex::Group*>(node);
            if (group && group->child_count() > 1) {
                group->child_at(1)->on_click([this, &val]() {
                    val = !val;
                    view_.update(model_);
                });
            }
        };

        setup_slider(view_.get_volume(), model_.volume);
        setup_slider(view_.get_brightness(), model_.brightness);
        setup_slider(view_.get_contrast(), model_.contrast);

        setup_toggle(view_.get_dark_mode(), model_.dark_mode);
        setup_toggle(view_.get_notifications(), model_.notifications);
        setup_toggle(view_.get_auto_save(), model_.auto_save);

        // Volume Control (Complex Nested)
        if (auto* n = view_.get_master_ctrl()) {
            auto* group = dynamic_cast<flex::Group*>(n);
            if (group && group->child_count() > 4) {
                group->child_at(1)->on_click([this]() {
                    model_.master_volume = std::fmod(model_.master_volume + 0.1f, 1.05f);
                    if (model_.master_volume > 1.0f) model_.master_volume = 0.0f;
                    view_.update(model_);
                });
                group->child_at(4)->on_click([this]() {
                    model_.master_muted = !model_.master_muted;
                    view_.update(model_);
                });
            }
        }

        // Settings Panel Rows
        setup_toggle(view_.get_s1(), model_.hdr_enabled);
        setup_toggle(view_.get_s2(), model_.vsync_enabled);
        setup_toggle(view_.get_s3(), model_.show_fps);
        setup_toggle(view_.get_s4(), model_.full_screen);
    }

    NestedModel& model_;
    NestedView& view_;
};

// ============================================================================
// Application Shell
// ============================================================================

class NestedComponentsDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Nested Components - MVC Demo",
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

        // Register components for DSL
        register_base_widgets();
        register_nested_components();

        std::cout << "Loading Nested Components Demo (MVC Refactored)...\\n";
        auto definition = flex::Definition::load_file("nested_components.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Initialize MVC
        model_ = std::make_unique<NestedModel>();
        view_ = std::make_unique<NestedView>(instance_);
        controller_ = std::make_unique<NestedController>(*model_, *view_);

        // Initial sync
        view_->update(*model_);

        std::cout << "Demo initialized! Click UI elements to interact.\\n";
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
            
            controller_->update(dt);
            instance_->advance(dt);
            
            render();
            SDL_Delay(16);
        }
    }

    ~NestedComponentsDemo() {
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

    flex::Instance::SharedPtr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    std::unique_ptr<NestedModel> model_;
    std::unique_ptr<NestedView> view_;
    std::unique_ptr<NestedController> controller_;

    bool running_ = false;

    void register_base_widgets() {
        // Slider
        auto slider_comp = flex::Component::create("Slider");
        slider_comp->add_prop("value", 0.5f);
        slider_comp->add_prop("width", 300.0f);
        slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
        slider_comp->set_builder([](const flex::Props& props) -> flex::ComponentNodePtr {
            auto slider = flex::Group::create();
            float val = flex::get_prop_float(props, "value", 0.5f);
            float w = flex::get_prop_float(props, "width", 300.0f);
            uint32_t col = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            auto track = flex::Shape::create();
            track->set_rect(w, 8.0f);
            track->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            track->set_position(0.0f, 6.0f);
            slider->add_child(track);

            auto fill = flex::Shape::create();
            fill->set_rect(w * val, 8.0f);
            uint8_t a = (col >> 24), r = (col >> 16), g = (col >> 8), b = col;
            fill->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
            fill->set_position(0.0f, 6.0f);
            slider->add_child(fill);

            auto thumb = flex::Shape::create();
            thumb->set_circle(10.0f);
            thumb->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
            thumb->set_position(w * val, 10.0f);
            slider->add_child(thumb);

            return slider;
        });
        flex::ComponentRegistry::instance().register_component(slider_comp);

        // ProgressBar
        auto progress_comp = flex::Component::create("ProgressBar");
        progress_comp->add_prop("progress", 0.5f);
        progress_comp->add_prop("width", 300.0f);
        progress_comp->add_prop("height", 20.0f);
        progress_comp->add_prop("color", uint32_t(0xFF198754));
        progress_comp->set_builder([](const flex::Props& props) -> flex::ComponentNodePtr {
            auto progress = flex::Group::create();
            float val = flex::get_prop_float(props, "progress", 0.5f);
            float w = flex::get_prop_float(props, "width", 300.0f);
            float h = flex::get_prop_float(props, "height", 20.0f);
            uint32_t col = flex::get_prop_color(props, "color", 0xFF198754);

            auto bg = flex::Shape::create();
            bg->set_rect(w, h);
            bg->set_fill(flex::Color(0.87f, 0.89f, 0.91f, 1.0f));
            progress->add_child(bg);

            auto fill = flex::Shape::create();
            fill->set_rect(w * val, h);
            uint8_t a = (col >> 24), r = (col >> 16), g = (col >> 8), b = col;
            fill->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
            progress->add_child(fill);

            return progress;
        });
        flex::ComponentRegistry::instance().register_component(progress_comp);

        // Toggle
        auto toggle_comp = flex::Component::create("Toggle");
        toggle_comp->add_prop("on", false);
        toggle_comp->add_prop("width", 50.0f);
        toggle_comp->add_prop("color", uint32_t(0xFF198754));
        toggle_comp->set_builder([](const flex::Props& props) -> flex::ComponentNodePtr {
            auto toggle = flex::Group::create();
            bool on = flex::get_prop_bool(props, "on");
            float w = flex::get_prop_float(props, "width", 50.0f);
            uint32_t col = flex::get_prop_color(props, "color", 0xFF198754);

            auto track = flex::Shape::create();
            track->set_rect(w, 26.0f);

            if (on) {
                uint8_t a = (col >> 24), r = (col >> 16), g = (col >> 8), b = col;
                track->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
            } else {
                track->set_fill(flex::Color(0.8f, 0.8f, 0.8f, 1.0f));
            }
            toggle->add_child(track);

            auto thumb = flex::Shape::create();
            thumb->set_circle(11.0f);
            thumb->set_fill(flex::Color(1, 1, 1, 1));
            thumb->set_position(on ? (w - 13.0f) : 13.0f, 13.0f);
            toggle->add_child(thumb);

            return toggle;
        });
        flex::ComponentRegistry::instance().register_component(toggle_comp);
    }

    void register_nested_components() {
        // LabeledSlider
        auto labeled_slider_comp = flex::Component::create("LabeledSlider");
        labeled_slider_comp->add_prop("label", std::string(""));
        labeled_slider_comp->add_prop("value", 0.5f);
        labeled_slider_comp->add_prop("width", 400.0f);
        labeled_slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
        labeled_slider_comp->set_builder([](const flex::Props& props) -> flex::ComponentNodePtr {
            auto group = flex::Group::create();
            std::string label = flex::get_prop_string(props, "label");
            float val = flex::get_prop_float(props, "value");
            float w = flex::get_prop_float(props, "width", 400.0f);
            uint32_t col = flex::get_prop_color(props, "color", 0xFF0D6EFD);

            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            group->add_child(label_text);

            auto slider = flex::create_component_instance("Slider", {{"value", val}, {"width", w}, {"color", col}});
            slider->set_position(0, 25);
            group->add_child(slider);

            auto value_text = flex::Text::create();
            std::ostringstream oss;
            oss << (int)(val * 100) << "%";
            value_text->set_content(oss.str());
            value_text->set_font_size(14.0f);
            value_text->set_position(w + 20, 30);
            group->add_child(value_text);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(labeled_slider_comp);

        // SettingsRow
        auto settings_row_comp = flex::Component::create("SettingsRow");
        settings_row_comp->add_prop("label", std::string(""));
        settings_row_comp->add_prop("on", false);
        settings_row_comp->set_builder([](const flex::Props& props) -> flex::ComponentNodePtr {
            auto group = flex::Group::create();
            std::string label = flex::get_prop_string(props, "label");
            bool on = flex::get_prop_bool(props, "on");

            auto label_text = flex::Text::create();
            label_text->set_content(label);
            label_text->set_font_size(14.0f);
            label_text->set_position(0, 10);
            group->add_child(label_text);

            auto toggle = flex::create_component_instance("Toggle", {{"on", on}});
            toggle->set_position(250, 0);
            group->add_child(toggle);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(settings_row_comp);

        // VolumeControl
        auto volume_ctrl_comp = flex::Component::create("VolumeControl");
        volume_ctrl_comp->add_prop("volume", 0.5f);
        volume_ctrl_comp->add_prop("muted", false);
        volume_ctrl_comp->set_builder([](const flex::Props& props) -> flex::ComponentNodePtr {
            auto group = flex::Group::create();
            float vol = flex::get_prop_float(props, "volume");
            bool muted = flex::get_prop_bool(props, "muted");

            auto title = flex::Text::create();
            title->set_content("Volume Control");
            title->set_font_size(16.0f);
            group->add_child(title);

            auto slider = flex::create_component_instance("Slider", {{"value", vol}, {"width", 400.0f}});
            slider->set_position(0, 30);
            group->add_child(slider);

            auto progress = flex::create_component_instance("ProgressBar", {{"progress", vol}, {"width", 400.0f}, {"height", 12.0f}});
            progress->set_position(0, 65);
            group->add_child(progress);

            auto mute_label = flex::Text::create();
            mute_label->set_content("Mute:");
            mute_label->set_font_size(14.0f);
            mute_label->set_position(0, 100);
            group->add_child(mute_label);

            auto toggle = flex::create_component_instance("Toggle", {{"on", muted}, {"color", uint32_t(0xFFDC3545)}});
            toggle->set_position(60, 95);
            group->add_child(toggle);

            return group;
        });
        flex::ComponentRegistry::instance().register_component(volume_ctrl_comp);
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
    NestedComponentsDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
