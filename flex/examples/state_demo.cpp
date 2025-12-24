/*
 * Flex State Management Demo - MVC Refactor
 * Demonstrates reactive UI with Observable State using the MVC pattern.
 */

#include "flex.h"

#include <SDL.h>
#include <thorvg.h>
#include <iostream>
#include <cmath>
#include <memory>
#include <vector>

// ============================================================================
// Component Registration
// ============================================================================

flex::Node::Ptr build_slider(const flex::Props& props) {
    auto slider = flex::Group::create();
    float value = flex::get_prop<float>(props, "value", 0.5f);
    float width = flex::get_prop<float>(props, "width", 300.0f);
    uint32_t color = flex::get_prop<uint32_t>(props, "color", 0xFF0D6EFD);

    float a = ((color >> 24) & 0xFF) / 255.0f;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    auto track = flex::Shape::create();
    track->set_rect(width, 8);
    track->set_fill(flex::Color(0.9f, 0.9f, 0.9f, 1.0f));
    track->set_y(6);
    slider->add_child(track);

    auto fill = flex::Shape::create();
    fill->set_rect(width * value, 8);
    fill->set_fill(flex::Color(r, g, b, a));
    fill->set_y(6);
    slider->add_child(fill);

    auto thumb = flex::Shape::create();
    thumb->set_circle(10);
    thumb->set_fill(flex::Color(r, g, b, a));
    thumb->set_position(width * value, 10);
    slider->add_child(thumb);

    return slider;
}

flex::Node::Ptr build_progress_bar(const flex::Props& props) {
    auto bar = flex::Group::create();
    float progress = flex::get_prop<float>(props, "progress", 0.5f);
    float width = flex::get_prop<float>(props, "width", 300.0f);
    uint32_t color = flex::get_prop<uint32_t>(props, "color", 0xFF198754);

    float a = ((color >> 24) & 0xFF) / 255.0f;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    auto bg = flex::Shape::create();
    bg->set_rect(width, 20);
    bg->set_fill(flex::Color(0.9f, 0.9f, 0.9f, 1.0f));
    bar->add_child(bg);

    auto fill = flex::Shape::create();
    fill->set_rect(width * progress, 20);
    fill->set_fill(flex::Color(r, g, b, a));
    bar->add_child(fill);

    return bar;
}

void register_components() {
    auto slider_comp = flex::Component::create("Slider");
    slider_comp->add_prop("value", 0.5f);
    slider_comp->add_prop("width", 300.0f);
    slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
    slider_comp->set_builder(build_slider);
    flex::ComponentRegistry::instance().register_component(slider_comp);

    auto progress_comp = flex::Component::create("ProgressBar");
    progress_comp->add_prop("progress", 0.5f);
    progress_comp->add_prop("width", 300.0f);
    progress_comp->add_prop("color", uint32_t(0xFF198754));
    progress_comp->set_builder(build_progress_bar);
    flex::ComponentRegistry::instance().register_component(progress_comp);
}

// ============================================================================
// Model - System State
// ============================================================================

class MetricsModel {
public:
    MetricsModel() {
        state_ = flex::ObservableState::create();
        state_->set("cpu_usage", 0.3f);
        state_->set("memory_usage", 0.65f);
        state_->set("disk_usage", 0.82f);
        state_->set("network_speed", 0.45f);
    }

    void update(float time) {
        state_->begin_batch();
        state_->set("cpu_usage", 0.5f + 0.3f * std::sin(time * 0.8f));
        state_->set("memory_usage", 0.6f + 0.2f * std::sin(time * 1.2f));
        state_->set("disk_usage", 0.75f + 0.15f * std::sin(time * 0.5f));
        state_->set("network_speed", 0.4f + 0.35f * std::sin(time * 1.5f));
        state_->end_batch();
    }

    flex::ObservableState::Ptr state() const { return state_; }

private:
    flex::ObservableState::Ptr state_;
};

// ============================================================================
// View - UI Presentation
// ============================================================================

class MetricsView {
public:
    MetricsView(flex::Instance::Ptr instance, flex::ObservableState::Ptr state) 
        : instance_(instance), state_(state) 
    {
        build_ui();
    }

    void build_ui() {
        auto artboard = instance_->artboard();

        // Background
        auto bg = flex::Shape::create();
        bg->set_rect(1200, 800);
        bg->set_fill(flex::Color(0.97f, 0.97f, 0.98f, 1.0f));
        artboard->add_child(bg);

        // Header
        auto title = flex::Text::create();
        title->set_content("Reactive State Management Demo (MVC)");
        title->set_font_size(32);
        title->set_position(50, 40);
        artboard->add_child(title);

        auto subtitle = flex::Text::create();
        subtitle->set_content("State changes automatically update all bound components via MVC pattern");
        subtitle->set_font_size(16);
        subtitle->set_color(flex::Color(0.5f, 0.5f, 0.5f, 1.0f));
        subtitle->set_position(50, 80);
        artboard->add_child(subtitle);

        // Metrics Groups
        add_metric_group("CPU Usage", "cpu_usage", 0xFF0D6EFD, 140);
        add_metric_group("Memory Usage", "memory_usage", 0xFFDC3545, 290);
        add_metric_group("Disk Usage", "disk_usage", 0xFFFFC107, 440);
        add_metric_group("Network Speed", "network_speed", 0xFF198754, 590);

        // Info Panel
        auto info = flex::Text::create();
        info->set_content(
            "Watch the UI update automatically!\n\n"
            "State changes every frame:\n"
            "- Both slider and progress bar update\n"
            "- Multiple components bound to same state\n"
            "- Zero manual UI updates needed\n\n"
            "This is MVC with Reactive Bindings!"
        );
        info->set_font_size(14);
        info->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        info->set_position(550, 140);
        artboard->add_child(info);
    }

    void render(flex::Renderer& renderer) {
        instance_->render(renderer);
    }

private:
    void add_metric_group(const std::string& label, const std::string& state_key, uint32_t color, float y) {
        auto group = flex::ReactiveGroup::create(state_);
        group->set_position(50, y);

        auto txt = flex::Text::create();
        txt->set_content(label);
        txt->set_font_size(18);
        group->add_child(txt);

        group->add_bound_component(
            "Slider",
            {{"value", 0.5f}, {"width", 400.0f}, {"color", color}},
            {{state_key, "value"}}, 
            0, 40
        );

        group->add_bound_component(
            "ProgressBar",
            {{"progress", 0.5f}, {"width", 400.0f}, {"color", color}},
            {{state_key, "progress"}},
            0, 90
        );

        instance_->artboard()->add_child(group);
    }

    flex::Instance::Ptr instance_;
    flex::ObservableState::Ptr state_;
};

// ============================================================================
// Controller - Coordination
// ============================================================================

class MetricsController {
public:
    MetricsController(MetricsModel& model, MetricsView& view, flex::Instance::Ptr instance) 
        : model_(model), view_(view), instance_(instance) {}

    void update(float dt) {
        time_ += dt;
        model_.update(time_);
        instance_->advance(dt);
    }

private:
    MetricsModel& model_;
    MetricsView& view_;
    flex::Instance::Ptr instance_;
    float time_ = 0;
};

// ============================================================================
// Application Shell
// ============================================================================

class StateDemo {
public:
    bool init() {
        flex::init();
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }
        
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

        window_ = SDL_CreateWindow("Flex State Demo - MVC", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 800, SDL_WINDOW_SHOWN);
        if (!window_) return false;

        register_components();

        instance_ = flex::Instance::create(1200, 800);
        model_ = std::make_unique<MetricsModel>();
        view_ = std::make_unique<MetricsView>(instance_, model_->state());
        controller_ = std::make_unique<MetricsController>(*model_, *view_, instance_);

        if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

        window_surface_ = SDL_GetWindowSurface(window_);
        offscreen_surface_ = SDL_CreateRGBSurface(0, 1200, 800, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        
        canvas_ = tvg::SwCanvas::gen();
        canvas_->target(static_cast<uint32_t*>(offscreen_surface_->pixels), 1200, 1200, 800, tvg::ColorSpace::ARGB8888);
        renderer_ = flex::create_thorvg_renderer(canvas_);

        return true;
    }

    void run() {
        bool running = true;
        SDL_Event event;
        Uint32 last_time = SDL_GetTicks();

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                    running = false;
                }
            }

            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;

            controller_->update(dt);

            renderer_->begin_frame(1200, 800, 1.0f);
            view_->render(*renderer_);
            renderer_->end_frame();

            SDL_BlitSurface(offscreen_surface_, nullptr, window_surface_, nullptr);
            SDL_UpdateWindowSurface(window_);
            SDL_Delay(16);
        }
    }

    ~StateDemo() {
        renderer_.reset();
        if (canvas_) delete canvas_;
        if (offscreen_surface_) SDL_FreeSurface(offscreen_surface_);
        tvg::Initializer::term();
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        flex::shutdown();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* window_surface_ = nullptr;
    SDL_Surface* offscreen_surface_ = nullptr;
    tvg::SwCanvas* canvas_ = nullptr;
    std::unique_ptr<flex::Renderer> renderer_;
    flex::Instance::Ptr instance_;
    std::unique_ptr<MetricsModel> model_;
    std::unique_ptr<MetricsView> view_;
    std::unique_ptr<MetricsController> controller_;
};

int main(int argc, char** argv) {
    StateDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
