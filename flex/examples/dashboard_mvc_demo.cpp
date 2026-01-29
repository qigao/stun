/*
 * Dashboard MVC Demo
 * Interactive dashboard demonstrating Model-View-Controller pattern with Flex DSL
 *
 * Architecture:
 *   Model      = DashboardModel (C++ struct holding data)
 *   View       = dashboard_mvc.flex (DSL scene)
 *   Controller = DashboardController (C++ class handling input and state machine)
 */

#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <chrono>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include <stb_sprintf.h>
#include "flex/backends/thorvg/init.h"
 

// ============================================================================
// Model - Dashboard State
// ============================================================================

struct MetricData {
    std::string label;
    std::string value;
    std::string delta;
    float numeric_value;
};

struct DashboardModel {
    // Metrics
    MetricData revenue  { "Revenue",      "$45,231", "+20%", 45231.0f };
    MetricData users    { "Active Users", "2,540",   "+12%", 2540.0f };
    MetricData bounce   { "Bounce Rate",  "42.3%",   "-5%",  42.3f };
    MetricData sessions { "Sessions",     "8,420",   "+8%",  8420.0f };

    // Chart data (7 days)
    float chart_values[7] = { 120, 180, 240, 160, 250, 200, 140 };

    // State flags
    bool is_loading = false;
    bool is_refreshing = false;
    bool has_error = false;
    bool show_notification = false;
    float notification_timer = 0.0f;

    // Simulation
    float update_timer = 0.0f;
    std::mt19937 rng{ std::random_device{}() };

    void simulate_data_update() {
        std::uniform_real_distribution<float> dist(-0.05f, 0.08f);

        revenue.numeric_value *= (1.0f + dist(rng));
        users.numeric_value *= (1.0f + dist(rng));
        sessions.numeric_value *= (1.0f + dist(rng));

        // Update display strings
        char buf[32];
        stbsp_snprintf(buf, sizeof(buf), "$%.0f", revenue.numeric_value);
        revenue.value = buf;

        stbsp_snprintf(buf, sizeof(buf), "%.0f", users.numeric_value);
        users.value = buf;

        stbsp_snprintf(buf, sizeof(buf), "%.0f", sessions.numeric_value);
        sessions.value = buf;

        // Shift chart data
        for (int i = 0; i < 6; ++i) {
            chart_values[i] = chart_values[i + 1];
        }
        std::uniform_real_distribution<float> chart_dist(100.0f, 280.0f);
        chart_values[6] = chart_dist(rng);
    }
};

// ============================================================================
// View - UI Binding
// ============================================================================

class DashboardView {
public:
    DashboardView(flex::Instance::Ptr instance) : instance_(instance) {
        auto* scene = instance_->scene();
        if (!scene) return;

        // Find metrics_row and get metric_card children
        auto* metrics_row = scene->find("metrics_row");
        if (metrics_row && metrics_row->is_group()) {
            auto* group = static_cast<flex::Group*>(metrics_row);
            const auto& children = group->children();
            for (size_t i = 0; i < children.size() && i < 4; ++i) {
                metric_cards_[i] = children[i] ;
            }
        }

        // Find chart_bars and get bar children
        auto* chart_bars = scene->find("chart_bars");
        if (chart_bars && chart_bars->is_group()) {
            auto* group = static_cast<flex::Group*>(chart_bars);
            const auto& children = group->children();
            for (size_t i = 0; i < children.size() && i < 7; ++i) {
                // Each bar_group contains a bar Shape as first child
                auto* bar_group = children[i] ;
                if (bar_group && bar_group->is_group()) {
                    auto* bg = static_cast<flex::Group*>(bar_group);
                    if (!bg->children().empty()) {
                        chart_bars_[i] = dynamic_cast<flex::Shape*>(bg->children()[0]);
                    }
                }
            }
        }

        // Find status elements
        status_dot_ = dynamic_cast<flex::Shape*>(scene->find("status_dot"));
        status_text_ = dynamic_cast<flex::Text*>(scene->find("status_text"));
        last_update_ = dynamic_cast<flex::Text*>(scene->find("last_update"));
        refresh_indicator_ = scene->find("refresh_indicator");
        toast_ = scene->find("toast");
    }

    void update(const DashboardModel& model) {
        // Update chart bar heights based on model data
        for (int i = 0; i < 7; ++i) {
            if (chart_bars_[i]) {
                float height = model.chart_values[i] * 0.8f;
                // Shape uses set_rect(width, height, corner_radius)
                chart_bars_[i]->set_rect(48, height, 0);
            }
        }

        // Update refresh indicator visibility
        if (refresh_indicator_) {
            refresh_indicator_->set_opacity(model.is_refreshing ? 1.0f : 0.0f);
        }

        // Update status
        if (model.has_error) {
            if (status_dot_) status_dot_->set_fill(0xda3633ff);
            if (status_text_) status_text_->set_content("Connection error");
        } else {
            if (status_dot_) status_dot_->set_fill(0x238636ff);
            if (status_text_) status_text_->set_content("All systems operational");
        }

        // Update last update time
        if (last_update_) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            char buf[64];
            strftime(buf, sizeof(buf), "Last updated: %H:%M:%S", localtime(&time));
            last_update_->set_content(buf);
        }

        // Update toast visibility
        if (toast_) {
            toast_->set_opacity(model.show_notification ? 1.0f : 0.0f);
        }
    }

    void show_cards_animation() {
        for (int i = 0; i < 4; ++i) {
            if (metric_cards_[i]) {
                metric_cards_[i]->set_opacity(1.0f);
            }
        }
        for (int i = 0; i < 7; ++i) {
            if (chart_bars_[i]) {
                chart_bars_[i]->set_opacity(1.0f);
            }
        }
    }

private:
    flex::Instance::Ptr instance_;
    flex::Node* metric_cards_[4] = {};
    flex::Shape* chart_bars_[7] = {};
    flex::Shape* status_dot_ = nullptr;
    flex::Text* status_text_ = nullptr;
    flex::Text* last_update_ = nullptr;
    flex::Node* refresh_indicator_ = nullptr;
    flex::Node* toast_ = nullptr;
};

// ============================================================================
// Controller - Logic and State Machine
// ============================================================================

class DashboardController {
public:
    DashboardController(DashboardModel& model, DashboardView& view, flex::Instance::Ptr instance)
        : model_(model), view_(view), instance_(instance) {}

    void update(float dt) {
        // Auto-refresh every 5 seconds
        model_.update_timer += dt;
        if (model_.update_timer >= 5.0f) {
            model_.update_timer = 0.0f;
            refresh_data();
        }

        // Handle notification timer
        if (model_.show_notification) {
            model_.notification_timer += dt;
            if (model_.notification_timer >= 3.0f) {
                model_.show_notification = false;
                model_.notification_timer = 0.0f;
            }
        }

        // Update view
        view_.update(model_);
    }

    void refresh_data() {
        model_.is_refreshing = true;
        model_.simulate_data_update();

        // Show notification
        model_.show_notification = true;
        model_.notification_timer = 0.0f;

        model_.is_refreshing = false;
    }

    void handle_key(SDL_Keycode key) {
        switch (key) {
            case SDLK_r:
                refresh_data();
                break;
            case SDLK_e:
                model_.has_error = !model_.has_error;
                break;
            case SDLK_n:
                model_.show_notification = true;
                model_.notification_timer = 0.0f;
                break;
        }
    }

    void on_loaded() {
        view_.show_cards_animation();
    }

private:
    DashboardModel& model_;
    DashboardView& view_;
    flex::Instance::Ptr instance_;
};

// ============================================================================
// Application
// ============================================================================

class DashboardMVCDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Dashboard MVC Demo - Press R:Refresh E:Toggle Error N:Notification ESC:Quit",
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

        // Load fonts
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        // Load the .flex file
        auto definition = flex::Definition::load_file("dashboard_mvc.flex");
        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->scene()) {
            std::cerr << "Failed to create scene\n";
            return false;
        }

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Initialize MVC components
        model_ = std::make_unique<DashboardModel>();
        view_ = std::make_unique<DashboardView>(instance_);
        controller_ = std::make_unique<DashboardController>(*model_, *view_, instance_);

        // Trigger state machine to show content via inputs
        // State machine transitions: idle -> loading (fetchData>0) -> loaded (dataReady>0)
        instance_->set_input("fetchData", 1.0f);  // Trigger idle -> loading
        instance_->advance(0.01f);                 // Process transition
        instance_->set_input("dataReady", 1.0f);  // Trigger loading -> loaded
        instance_->advance(0.01f);                 // Process transition

        // Also manually show cards in case animation doesn't work
        controller_->on_loaded();

        std::cout << "Dashboard MVC Demo initialized!\n";
        std::cout << "Keys: R=Refresh, E=Toggle Error, N=Notification, ESC=Quit\n";

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

            instance_->advance(dt);
            controller_->update(dt);

            render();
            SDL_Delay(16);
        }
    }

    ~DashboardMVCDemo() {
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
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 800;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    std::unique_ptr<DashboardModel> model_;
    std::unique_ptr<DashboardView> view_;
    std::unique_ptr<DashboardController> controller_;

    bool running_ = false;

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
                    } else {
                        controller_->handle_key(event.key.keysym.sym);
                    }
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
    DashboardMVCDemo demo;
    if (!demo.init()) {
        std::cerr << "Failed to initialize demo\n";
        return 1;
    }
    demo.run();
    return 0;
}
