#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>

#include <SDL2/SDL.h>
#include <thorvg.h>

#include <flex/flex.h>
#include <flex/state.h>
#include <flex/component.h>
#include <flex/state_binding.h>
#include <flex/group.h>
#include <flex/shape.h>
#include <flex/text.h>

// ============================================================================
// Constants and Utilities
// ============================================================================

const float SEEK_BAR_WIDTH = 300.0f;
const float VOLUME_SLIDER_WIDTH = 150.0f;
const float SEEK_BAR_ABS_X = 450.0f; 
const float VOLUME_SLIDER_ABS_X = 450.0f;

std::string format_time(float seconds) {
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    std::ostringstream oss;
    oss << mins << ":" << std::setfill('0') << std::setw(2) << secs;
    return oss.str();
}

// ============================================================================
// Component Builders
// ============================================================================

void register_music_player_components() {
    // Album Cover
    auto cover_comp = flex::Component::create("AlbumCover");
    cover_comp->add_prop("size", 300.0f);
    cover_comp->add_prop("color1", uint32_t(0xFF8E2DE2));
    cover_comp->add_prop("color2", uint32_t(0xFF4A00E0));
    cover_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
        float size = flex::get_prop_float(props, "size", 300.0f);
        uint32_t color1 = flex::get_prop_color(props, "color1", 0xFF8E2DE2);
        uint32_t color2 = flex::get_prop_color(props, "color2", 0xFF4A00E0);

        auto group = flex::Group::create();
        
        auto bg = flex::Shape::create();
        bg->set_rect(size, size, 20.0f);
        uint8_t a1 = (color1 >> 24), r1 = (color1 >> 16), g1 = (color1 >> 8), b1 = color1;
        bg->set_fill(flex::Color(r1/255.f, g1/255.f, b1/255.f, a1/255.f));
        group->add_child(bg);

        auto inner = flex::Shape::create();
        inner->set_rect(size - 40, size - 40, 10.0f);
        inner->set_position(20, 20);
        uint8_t a2 = (color2 >> 24), r2 = (color2 >> 16), g2 = (color2 >> 8), b2 = color2;
        inner->set_fill(flex::Color(r2/255.f, g2/255.f, b2/255.f, a2/255.f));
        inner->set_opacity(0.5f);
        group->add_child(inner);

        return group;
    });
    flex::ComponentRegistry::instance().register_component(cover_comp);

    // Seek Bar
    auto seek_comp = flex::Component::create("SeekBar");
    seek_comp->add_prop("progress", 0.0f);
    seek_comp->add_prop("width", 300.0f);
    seek_comp->add_prop("color", uint32_t(0xFF1DB954));
    seek_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
        float progress = flex::get_prop_float(props, "progress", 0.0f);
        float width = flex::get_prop_float(props, "width", 300.0f);
        uint32_t color = flex::get_prop_color(props, "color", 0xFF1DB954);

        auto group = flex::Group::create();

        auto track = flex::Shape::create();
        track->set_rect(width, 4.0f, 2.0f);
        track->set_fill(flex::Color(0.3f, 0.3f, 0.35f, 1.0f));
        track->set_position(0, 0);
        group->add_child(track);

        auto fill = flex::Shape::create();
        fill->set_rect(width * progress, 4.0f, 2.0f);
        uint8_t a = (color >> 24), r = (color >> 16), g = (color >> 8), b = color;
        fill->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
        fill->set_position(0, 0);
        group->add_child(fill);

        auto thumb = flex::Shape::create();
        thumb->set_circle(6.0f);
        thumb->set_fill(flex::Color(1, 1, 1, 1));
        thumb->set_position(width * progress, 2.0f);
        group->add_child(thumb);

        return group;
    });
    flex::ComponentRegistry::instance().register_component(seek_comp);

    // Volume Slider
    auto volume_comp = flex::Component::create("VolumeSlider");
    volume_comp->add_prop("value", 0.7f);
    volume_comp->add_prop("width", 150.0f);
    volume_comp->add_prop("color", uint32_t(0xFFFFFFFF));
    volume_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
        float value = flex::get_prop_float(props, "value", 0.7f);
        float width = flex::get_prop_float(props, "width", 150.0f);
        uint32_t color = flex::get_prop_color(props, "color", 0xFFFFFFFF);

        auto group = flex::Group::create();

        auto track = flex::Shape::create();
        track->set_rect(width, 4.0f, 2.0f);
        track->set_fill(flex::Color(0.3f, 0.3f, 0.35f, 1.0f));
        group->add_child(track);

        auto fill = flex::Shape::create();
        fill->set_rect(width * value, 4.0f, 2.0f);
        uint8_t a = (color >> 24), r = (color >> 16), g = (color >> 8), b = color;
        fill->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
        group->add_child(fill);

        auto thumb = flex::Shape::create();
        thumb->set_circle(6.0f);
        thumb->set_fill(flex::Color(1, 1, 1, 1));
        thumb->set_position(width * value, 2.0f);
        group->add_child(thumb);

        return group;
    });
    flex::ComponentRegistry::instance().register_component(volume_comp);

    // Play Button
    auto play_comp = flex::Component::create("PlayButton");
    play_comp->add_prop("playing", false);
    play_comp->add_prop("color", uint32_t(0xFF1DB954));
    play_comp->set_builder([](const flex::Props& props) -> std::shared_ptr<flex::Node> {
        bool playing = flex::get_prop_bool(props, "playing", false);
        uint32_t color = flex::get_prop_color(props, "color", 0xFF1DB954);

        auto group = flex::Group::create();

        auto circle = flex::Shape::create();
        circle->set_circle(30.0f);
        uint8_t a = (color >> 24), r = (color >> 16), g = (color >> 8), b = color;
        circle->set_fill(flex::Color(r/255.f, g/255.f, b/255.f, a/255.f));
        group->add_child(circle);

        if (playing) {
            auto bar1 = flex::Shape::create();
            bar1->set_rect(6, 20, 1);
            bar1->set_fill(flex::Color(1, 1, 1, 1));
            bar1->set_position(-7, -10);
            group->add_child(bar1);

            auto bar2 = flex::Shape::create();
            bar2->set_rect(6, 20, 1);
            bar2->set_fill(flex::Color(1, 1, 1, 1));
            bar2->set_position(1, -10);
            group->add_child(bar2);
        } else {
            auto tri = flex::Shape::create();
            tri->set_path("M -5 -10 L 10 0 L -5 10 Z");
            tri->set_fill(flex::Color(1, 1, 1, 1));
            tri->set_position(2, 0); // Slight offset to look centered
            group->add_child(tri);
        }

        return group;
    });
    flex::ComponentRegistry::instance().register_component(play_comp);
}

// ============================================================================
// Model - Music Player State and Logic
// ============================================================================

class MusicPlayerModel {
public:
    MusicPlayerModel() {
        state_ = flex::ObservableState::create();
        
        // Initial values
        state_->set_string("song_title", "Neon Lights");
        state_->set_string("song_artist", "Synthwave Dreams");
        state_->set_float("duration", 185.0f);
        state_->set_bool("playing", false);
        state_->set_float("current_time", 0.0f);
        state_->set_float("volume", 0.7f);
        state_->set_float("progress", 0.0f);

        // Derived: progress = current_time / duration
        state_->watch("current_time", [this](const std::string&, const flex::StateValue& val) {
            float current = std::get<float>(val);
            float duration = state_->get_float("duration", 185.0f);
            state_->set_float("progress", current / duration);
        });
    }

    void toggle_play() {
        bool playing = state_->get_bool("playing", false);
        state_->set_bool("playing", !playing);
        std::cout << (playing ? "⏸ Paused" : "▶ Playing") << std::endl;
    }

    void seek(float progress) {
        float duration = state_->get_float("duration", 185.0f);
        state_->set_float("current_time", std::max(0.0f, std::min(1.0f, progress)) * duration);
    }

    void set_volume(float volume) {
        float vol = std::max(0.0f, std::min(1.0f, volume));
        state_->set_float("volume", vol);
        std::cout << "🔊 Volume: " << (int)(vol * 100) << "%" << std::endl;
    }

    void update(float dt) {
        if (state_->get_bool("playing", false)) {
            float current = state_->get_float("current_time", 0.0f);
            float duration = state_->get_float("duration", 185.0f);
            current += dt;
            if (current >= duration) current = 0.0f;
            state_->set_float("current_time", current);
        }
    }

    flex::ObservableState::Ptr state() const { return state_; }

private:
    flex::ObservableState::Ptr state_;
};

// ============================================================================
// View - UI Components and Layout
// ============================================================================

class MusicPlayerView {
public:
    MusicPlayerView(flex::ObservableState::Ptr state) : state_(state) {
        instance_ = flex::Instance::create(800, 600);
        auto artboard = instance_->artboard();

        // Background
        auto bg = flex::Shape::create();
        bg->set_rect(800, 600);
        bg->set_fill(flex::Color(0.1f, 0.1f, 0.12f, 1.0f));
        artboard->add_child(bg);

        // Album Cover
        auto cover_group = flex::ReactiveGroup::create(state_);
        cover_group->set_position(100, 150);
        cover_group->add_bound_component(
            "AlbumCover",
            {{"size", 300.0f}, {"color1", uint32_t(0xFF8E2DE2)}, {"color2", uint32_t(0xFF4A00E0)}},
            {}, 0, 0
        );
        artboard->add_child(cover_group);

        // Controls Group
        controls_ = flex::Group::create();
        controls_->set_position(450, 150);
        artboard->add_child(controls_);

        // Song Info
        title_ = flex::Text::create();
        title_->set_content(state_->get_string("song_title", "Unknown"));
        title_->set_font_size(28);
        title_->set_color(flex::Color(1, 1, 1, 1));
        title_->set_position(0, 0);
        controls_->add_child(title_);

        artist_ = flex::Text::create();
        artist_->set_content(state_->get_string("song_artist", "Unknown"));
        artist_->set_font_size(18);
        artist_->set_color(flex::Color(0.7f, 0.7f, 0.7f, 1));
        artist_->set_position(0, 40);
        controls_->add_child(artist_);

        // Seek Bar
        seek_bar_ = flex::create_component_instance(
            "SeekBar",
            {{"progress", 0.0f}, {"width", SEEK_BAR_WIDTH}, {"color", uint32_t(0xFF1DB954)}}
        );
        seek_bar_->set_position(0, 100);
        controls_->add_child(seek_bar_);

        // Time Label
        time_label_ = flex::Text::create();
        time_label_->set_content("0:00 / 0:00");
        time_label_->set_font_size(14);
        time_label_->set_color(flex::Color(0.6f, 0.6f, 0.6f, 1));
        time_label_->set_position(0, 125);
        controls_->add_child(time_label_);

        // Play Button Binding
        play_binding_ = std::make_shared<flex::StateComponentBinding>(
            state_, "PlayButton",
            flex::Props{{"playing", false}, {"color", uint32_t(0xFF1DB954)}}
        );
        play_binding_->bind("playing", "playing");
        
        play_button_ = play_binding_->component();
        play_button_->set_position(130, 160);
        controls_->add_child(play_button_);

        // Volume Label & Slider
        auto vol_label = flex::Text::create();
        vol_label->set_content("Volume");
        vol_label->set_font_size(14);
        vol_label->set_color(flex::Color(0.7f, 0.7f, 0.7f, 1));
        vol_label->set_position(0, 240);
        controls_->add_child(vol_label);

        volume_slider_ = flex::create_component_instance(
            "VolumeSlider",
            {{"value", 0.7f}, {"width", VOLUME_SLIDER_WIDTH}, {"color", uint32_t(0xFFFFFFFF)}}
        );
        volume_slider_->set_position(0, 265);
        controls_->add_child(volume_slider_);
    }

    void update_ui() {
        float current = state_->get_float("current_time", 0.0f);
        float duration = state_->get_float("duration", 1.0f);
        float progress = current / duration;

        // Update Seek Bar
        static float last_progress = -1.0f;
        if (std::abs(progress - last_progress) > 0.001f) {
            auto new_seek_bar = flex::create_component_instance(
                "SeekBar",
                {{"progress", progress}, {"width", SEEK_BAR_WIDTH}, {"color", uint32_t(0xFF1DB954)}}
            );
            new_seek_bar->set_position(0, 100);
            controls_->remove_child(seek_bar_.get());
            controls_->insert_child(new_seek_bar, 2);
            seek_bar_ = new_seek_bar;
            
            time_label_->set_content(format_time(current) + " / " + format_time(duration));
            last_progress = progress;
            
            if (on_seek_bar_rebuilt) on_seek_bar_rebuilt(seek_bar_);
        }

        // Update Volume Slider
        static float last_vol = -1.0f;
        float vol = state_->get_float("volume", 0.7f);
        if (std::abs(vol - last_vol) > 0.001f) {
            auto new_vol_slider = flex::create_component_instance(
                "VolumeSlider",
                {{"value", vol}, {"width", VOLUME_SLIDER_WIDTH}, {"color", uint32_t(0xFFFFFFFF)}}
            );
            new_vol_slider->set_position(0, 265);
            controls_->remove_child(volume_slider_.get());
            controls_->add_child(new_vol_slider);
            volume_slider_ = new_vol_slider;
            last_vol = vol;

            if (on_volume_rebuilt) on_volume_rebuilt(volume_slider_);
        }
    }

    flex::Instance::Ptr instance() const { return instance_; }
    flex::Node::Ptr play_button() const { return play_button_; }
    flex::Node::Ptr seek_bar() const { return seek_bar_; }
    flex::Node::Ptr volume_slider() const { return volume_slider_; }
    std::shared_ptr<flex::StateComponentBinding> play_binding() { return play_binding_; }
    flex::Group::Ptr controls() { return controls_; }

    std::function<void(flex::Node::Ptr)> on_seek_bar_rebuilt;
    std::function<void(flex::Node::Ptr)> on_volume_rebuilt;

private:
    flex::ObservableState::Ptr state_;
    flex::Instance::Ptr instance_;
    flex::Group::Ptr controls_;
    flex::Text::Ptr title_;
    flex::Text::Ptr artist_;
    flex::Text::Ptr time_label_;
    flex::Node::Ptr seek_bar_;
    flex::Node::Ptr volume_slider_;
    flex::Node::Ptr play_button_;
    std::shared_ptr<flex::StateComponentBinding> play_binding_;
};

// ============================================================================
// Controller - Interaction and Glue
// ============================================================================

class MusicPlayerController {
public:
    MusicPlayerController(MusicPlayerModel& model, MusicPlayerView& view)
        : model_(model), view_(view) {
        
        setup_events();

        // Handle reactive rebuilds
        view_.on_seek_bar_rebuilt = [this](flex::Node::Ptr node) { setup_seek_events(node); };
        view_.on_volume_rebuilt = [this](flex::Node::Ptr node) { setup_volume_events(node); };
        
        view_.play_binding()->on_rebuild([this](flex::Node::Ptr old_node, flex::Node::Ptr new_node) {
            view_.controls()->remove_child(old_node.get());
            new_node->set_position(130, 160);
            view_.controls()->insert_child(new_node, 4); // After time label
            setup_play_events(new_node);
        });
    }

    void handle_sdl_event(SDL_Event& event) {
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_SPACE: model_.toggle_play(); break;
                case SDLK_LEFT: {
                    float cur = model_.state()->get_float("current_time", 0);
                    model_.seek((cur - 5.0f) / model_.state()->get_float("duration", 185.0f));
                    break;
                }
                case SDLK_RIGHT: {
                    float cur = model_.state()->get_float("current_time", 0);
                    float dur = model_.state()->get_float("duration", 185.0f);
                    model_.seek((cur + 5.0f) / dur);
                    break;
                }
                case SDLK_UP: {
                    float vol = model_.state()->get_float("volume", 0.7f);
                    model_.set_volume(vol + 0.1f);
                    break;
                }
                case SDLK_DOWN: {
                    float vol = model_.state()->get_float("volume", 0.7f);
                    model_.set_volume(vol - 0.1f);
                    break;
                }
            }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            drag_state_.is_seeking = false;
            drag_state_.is_adjusting_volume = false;
        }
    }

    void handle_global_mouse(float x, float y) {
        if (drag_state_.is_seeking) {
            float local_x = x - SEEK_BAR_ABS_X;
            model_.seek(local_x / SEEK_BAR_WIDTH);
        }
        if (drag_state_.is_adjusting_volume) {
            float local_x = x - VOLUME_SLIDER_ABS_X;
            model_.set_volume(local_x / VOLUME_SLIDER_WIDTH);
        }
    }

private:
    struct DragState {
        bool is_seeking = false;
        bool is_adjusting_volume = false;
    };
    DragState drag_state_;

    void setup_events() {
        setup_play_events(view_.play_button());
        setup_seek_events(view_.seek_bar());
        setup_volume_events(view_.volume_slider());
    }

    void setup_play_events(flex::Node::Ptr node) {
        if (!node) return;
        node->on_click([this]() { model_.toggle_play(); });
    }

    void setup_seek_events(flex::Node::Ptr node) {
        if (!node) return;
        node->on_pointer_down([this](flex::PointerEvent& e) {
            drag_state_.is_seeking = true;
            model_.seek(e.local_x / SEEK_BAR_WIDTH);
        });
        node->on_pointer_up([this](flex::PointerEvent&) { drag_state_.is_seeking = false; });
    }

    void setup_volume_events(flex::Node::Ptr node) {
        if (!node) return;
        node->on_pointer_down([this](flex::PointerEvent& e) {
            drag_state_.is_adjusting_volume = true;
            model_.set_volume(e.local_x / VOLUME_SLIDER_WIDTH);
        });
        node->on_pointer_up([this](flex::PointerEvent&) { drag_state_.is_adjusting_volume = false; });
    }

    MusicPlayerModel& model_;
    MusicPlayerView& view_;
};

// ============================================================================
// Main Application Wrapper
// ============================================================================

class MusicPlayerApp {
public:
    bool init() {
        flex::init();
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

        window_ = SDL_CreateWindow("Flex Music Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
        if (!window_) return false;

        register_music_player_components();

        model_ = std::make_unique<MusicPlayerModel>();
        view_ = std::make_unique<MusicPlayerView>(model_->state());
        controller_ = std::make_unique<MusicPlayerController>(*model_, *view_);

        if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

        surface_ = SDL_GetWindowSurface(window_);
        canvas_ = std::unique_ptr<tvg::SwCanvas>(tvg::SwCanvas::gen());
        canvas_->target(static_cast<uint32_t*>(surface_->pixels), surface_->pitch / 4, surface_->w, surface_->h, tvg::ColorSpace::ARGB8888);
        renderer_ = flex::create_thorvg_renderer(canvas_.get());

        return true;
    }

    void run() {
        bool running = true;
        SDL_Event event;
        Uint32 last_time = SDL_GetTicks();

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    view_->instance()->send_pointer_event((float)event.button.x, (float)event.button.y, true);
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    view_->instance()->send_pointer_event((float)event.button.x, (float)event.button.y, false);
                } else if (event.type == SDL_MOUSEMOTION) {
                    view_->instance()->send_pointer_event((float)event.motion.x, (float)event.motion.y, event.motion.state & SDL_BUTTON_LMASK);
                    controller_->handle_global_mouse((float)event.motion.x, (float)event.motion.y);
                }
                controller_->handle_sdl_event(event);
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
            }

            Uint32 now = SDL_GetTicks();
            float dt = (now - last_time) / 1000.0f;
            last_time = now;

            model_->update(dt);
            view_->update_ui();

            renderer_->begin_frame(800, 600, 1.0f);
            view_->instance()->render(*renderer_);
            renderer_->end_frame();

            canvas_->draw();
            canvas_->sync();
            SDL_UpdateWindowSurface(window_);
            SDL_Delay(16);
        }
    }

    ~MusicPlayerApp() {
        tvg::Initializer::term();
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        flex::shutdown();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::unique_ptr<flex::Renderer> renderer_;
    std::unique_ptr<MusicPlayerModel> model_;
    std::unique_ptr<MusicPlayerView> view_;
    std::unique_ptr<MusicPlayerController> controller_;
};

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    MusicPlayerApp app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
