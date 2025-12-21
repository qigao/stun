/*
 * Flex Music Player Application
 *
 * A complete music player demo showcasing:
 * - ObservableState for reactive data binding
 * - Component System for reusable UI
 * - Event System for user interactions
 * - Smooth animations
 */

#include "flex/flex.h"
#include "flex/state.h"
#include "flex/state_binding.h"
#include "flex/component.h"
#include "flex/shape.h"
#include "flex/text.h"
#include "flex/group.h"
#include "flex/artboard.h"
#include "flex/renderer.h"
#include "flex/event.h"

#include <SDL.h>
#include <thorvg.h>
#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>

// ============================================================================
// Helper: Format time as MM:SS
// ============================================================================

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

// Play/Pause Button (circular button with triangle/bars)
flex::Node::Ptr build_play_button(const flex::Props& props) {
    auto button = flex::Group::create();

    bool is_playing = flex::get_prop<bool>(props, "playing", false);
    uint32_t color = flex::get_prop<uint32_t>(props, "color", 0xFF1DB954); // Spotify green

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    // Invisible clickable area (important for hit testing!)
    auto hit_area = flex::Shape::create();
    hit_area->set_rect(60, 60);
    hit_area->set_fill(flex::Color(0, 0, 0, 0)); // Transparent
    button->add_child(hit_area);

    // Circular background
    auto circle = flex::Shape::create();
    circle->set_circle(30);
    circle->set_fill(flex::Color(r, g, b, 1.0f));
    circle->set_position(30, 30);
    button->add_child(circle);

    if (is_playing) {
        // Pause icon (two vertical bars)
        auto bar1 = flex::Shape::create();
        bar1->set_rect(6, 20);
        bar1->set_fill(flex::Color(1, 1, 1, 1));
        bar1->set_position(20, 20);
        button->add_child(bar1);

        auto bar2 = flex::Shape::create();
        bar2->set_rect(6, 20);
        bar2->set_fill(flex::Color(1, 1, 1, 1));
        bar2->set_position(34, 20);
        button->add_child(bar2);
    } else {
        // Play icon (triangle)
        auto triangle = flex::Shape::create();
        triangle->set_polygon(3, 12); // 3 sides = triangle
        triangle->set_fill(flex::Color(1, 1, 1, 1));
        triangle->set_position(33, 30);
        triangle->set_rotation(90); // Point to the right
        button->add_child(triangle);
    }

    return button;
}

// Album Cover (square with gradient)
flex::Node::Ptr build_album_cover(const flex::Props& props) {
    auto cover = flex::Group::create();

    float size = flex::get_prop<float>(props, "size", 200.0f);
    uint32_t color1 = flex::get_prop<uint32_t>(props, "color1", 0xFF8E2DE2);
    uint32_t color2 = flex::get_prop<uint32_t>(props, "color2", 0xFF4A00E0);

    // Background square
    auto bg = flex::Shape::create();
    bg->set_rect(size, size);

    // Extract colors
    float r1 = ((color1 >> 16) & 0xFF) / 255.0f;
    float g1 = ((color1 >> 8) & 0xFF) / 255.0f;
    float b1 = (color1 & 0xFF) / 255.0f;

    // Use solid color for now (gradient would need LinearGradient support)
    bg->set_fill(flex::Color(r1, g1, b1, 1.0f));
    cover->add_child(bg);

    // Music note icon (simplified: circle + stem)
    auto note_circle = flex::Shape::create();
    note_circle->set_circle(15);
    note_circle->set_fill(flex::Color(1, 1, 1, 0.3f));
    note_circle->set_position(size * 0.6f, size * 0.7f);
    cover->add_child(note_circle);

    auto note_stem = flex::Shape::create();
    note_stem->set_rect(4, 40);
    note_stem->set_fill(flex::Color(1, 1, 1, 0.3f));
    note_stem->set_position(size * 0.6f + 11, size * 0.7f - 40);
    cover->add_child(note_stem);

    return cover;
}

// Progress Slider (seekable)
flex::Node::Ptr build_seek_bar(const flex::Props& props) {
    auto bar = flex::Group::create();

    float progress = flex::get_prop<float>(props, "progress", 0.5f);
    float width = flex::get_prop<float>(props, "width", 400.0f);
    uint32_t color = flex::get_prop<uint32_t>(props, "color", 0xFF1DB954);

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    // Invisible clickable area (important for hit testing!)
    auto hit_area = flex::Shape::create();
    hit_area->set_rect(width, 20);
    hit_area->set_fill(flex::Color(0, 0, 0, 0)); // Transparent
    bar->add_child(hit_area);

    // Background track
    auto track = flex::Shape::create();
    track->set_rect(width, 4);
    track->set_fill(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
    track->set_y(8);
    bar->add_child(track);

    // Filled portion
    auto fill = flex::Shape::create();
    fill->set_rect(width * progress, 4);
    fill->set_fill(flex::Color(r, g, b, 1.0f));
    fill->set_y(8);
    bar->add_child(fill);

    // Thumb (small circle)
    auto thumb = flex::Shape::create();
    thumb->set_circle(8);
    thumb->set_fill(flex::Color(r, g, b, 1.0f));
    thumb->set_position(width * progress, 10);
    bar->add_child(thumb);

    return bar;
}

// Volume Slider (vertical or horizontal)
flex::Node::Ptr build_volume_slider(const flex::Props& props) {
    auto slider = flex::Group::create();

    float value = flex::get_prop<float>(props, "value", 0.7f);
    float width = flex::get_prop<float>(props, "width", 150.0f);
    uint32_t color = flex::get_prop<uint32_t>(props, "color", 0xFFFFFFFF);

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    // Invisible clickable area (important for hit testing!)
    auto hit_area = flex::Shape::create();
    hit_area->set_rect(width, 20);
    hit_area->set_fill(flex::Color(0, 0, 0, 0)); // Transparent
    slider->add_child(hit_area);

    // Background track
    auto track = flex::Shape::create();
    track->set_rect(width, 6);
    track->set_fill(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
    track->set_y(7);
    slider->add_child(track);

    // Filled portion
    auto fill = flex::Shape::create();
    fill->set_rect(width * value, 6);
    fill->set_fill(flex::Color(r, g, b, 1.0f));
    fill->set_y(7);
    slider->add_child(fill);

    // Thumb
    auto thumb = flex::Shape::create();
    thumb->set_circle(7);
    thumb->set_fill(flex::Color(r, g, b, 1.0f));
    thumb->set_position(width * value, 10);
    slider->add_child(thumb);

    return slider;
}

// ============================================================================
// Event Setup Helpers (reusable for component rebuilds)
// ============================================================================

// Global drag state (shared across rebuilds)
struct DragState {
    bool is_seeking = false;
    bool is_adjusting_volume = false;
    float seek_bar_width = 300.0f;
    float volume_slider_width = 150.0f;
    float seek_bar_x = 0; // Absolute position
    float volume_slider_x = 0;
};

static DragState g_drag_state;

// UI layout constants
static const float SEEK_BAR_WIDTH = 300.0f;
static const float SEEK_BAR_ABS_X = 450.0f;
static const float VOLUME_SLIDER_WIDTH = 150.0f;
static const float VOLUME_SLIDER_ABS_X = 450.0f;

void setup_play_button_events(flex::Node::Ptr button, flex::ObservableState::Ptr state) {
    button->on_click([state]() {
        bool is_playing = state->get_bool("playing", false);
        state->set("playing", !is_playing);
        std::cout << (is_playing ? "⏸ Paused" : "▶ Playing") << std::endl;
    });
}

void setup_seek_bar_events(flex::Node::Ptr seek_bar, flex::ObservableState::Ptr state, float width, float abs_x) {
    g_drag_state.seek_bar_width = width;
    g_drag_state.seek_bar_x = abs_x;

    seek_bar->on_pointer_down([state, width](flex::PointerEvent& e) {
        g_drag_state.is_seeking = true;
        float local_x = e.local_x;
        float progress = std::max(0.0f, std::min(1.0f, local_x / width));
        float duration = state->get_float("duration", 185.0f);
        state->set("current_time", progress * duration);
    });

    seek_bar->on_pointer_up([](flex::PointerEvent& e) {
        g_drag_state.is_seeking = false;
    });
}

void setup_volume_slider_events(flex::Node::Ptr slider, flex::ObservableState::Ptr state, float width, float abs_x) {
    g_drag_state.volume_slider_width = width;
    g_drag_state.volume_slider_x = abs_x;

    slider->on_pointer_down([state, width](flex::PointerEvent& e) {
        g_drag_state.is_adjusting_volume = true;
        float local_x = e.local_x;
        float volume = std::max(0.0f, std::min(1.0f, local_x / width));
        state->set("volume", volume);
        std::cout << "🔊 Volume: " << (int)(volume * 100) << "%" << std::endl;
    });

    slider->on_pointer_up([](flex::PointerEvent& e) {
        g_drag_state.is_adjusting_volume = false;
    });
}

// Global mouse move handler (called from SDL event loop)
void handle_global_mouse_move(float mouse_x, float mouse_y, flex::ObservableState::Ptr state) {
    if (g_drag_state.is_seeking) {
        float local_x = mouse_x - g_drag_state.seek_bar_x;
        float progress = std::max(0.0f, std::min(1.0f, local_x / g_drag_state.seek_bar_width));
        float duration = state->get_float("duration", 185.0f);
        state->set("current_time", progress * duration);
    }

    if (g_drag_state.is_adjusting_volume) {
        float local_x = mouse_x - g_drag_state.volume_slider_x;
        float volume = std::max(0.0f, std::min(1.0f, local_x / g_drag_state.volume_slider_width));
        state->set("volume", volume);
        // Throttle console output slightly or just keep it for now
        std::cout << "🔊 Volume: " << (int)(volume * 100) << "%" << std::endl;
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_components() {
    // PlayButton component
    auto play_btn = flex::Component::create("PlayButton");
    play_btn->add_prop("playing", false);
    play_btn->add_prop("color", uint32_t(0xFF1DB954));
    play_btn->set_builder(build_play_button);
    flex::ComponentRegistry::instance().register_component(play_btn);

    // AlbumCover component
    auto album = flex::Component::create("AlbumCover");
    album->add_prop("size", 200.0f);
    album->add_prop("color1", uint32_t(0xFF8E2DE2));
    album->add_prop("color2", uint32_t(0xFF4A00E0));
    album->set_builder(build_album_cover);
    flex::ComponentRegistry::instance().register_component(album);

    // SeekBar component
    auto seek = flex::Component::create("SeekBar");
    seek->add_prop("progress", 0.5f);
    seek->add_prop("width", 400.0f);
    seek->add_prop("color", uint32_t(0xFF1DB954));
    seek->set_builder(build_seek_bar);
    flex::ComponentRegistry::instance().register_component(seek);

    // VolumeSlider component
    auto volume = flex::Component::create("VolumeSlider");
    volume->add_prop("value", 0.7f);
    volume->add_prop("width", 150.0f);
    volume->add_prop("color", uint32_t(0xFFFFFFFF));
    volume->set_builder(build_volume_slider);
    flex::ComponentRegistry::instance().register_component(volume);
}

// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // Initialize
    flex::init();
    if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
        flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Flex Music Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Register components
    register_components();

    // ============================================================================
    // Create State
    // ============================================================================

    auto player_state = flex::ObservableState::create();

    // Song metadata
    player_state->set("song_title", std::string("Neon Lights"));
    player_state->set("song_artist", std::string("Synthwave Dreams"));
    player_state->set("duration", 185.0f); // 3:05

    // Playback state
    player_state->set("playing", false);
    player_state->set("current_time", 0.0f);
    player_state->set("volume", 0.7f);

    // Derived: progress = current_time / duration
    player_state->set("progress", 0.0f);

    // Auto-update progress when current_time changes
    player_state->watch("current_time", [player_state](const std::string&, const flex::StateValue& val) {
        float current = std::get<float>(val);
        float duration = player_state->get_float("duration", 185.0f);
        player_state->set("progress", current / duration);
    });

    // ============================================================================
    // Build UI
    // ============================================================================

    auto instance = flex::Instance::create(800, 600);
    auto artboard = instance->artboard();

    // Background (dark theme)
    auto bg = flex::Shape::create();
    bg->set_rect(800, 600);
    bg->set_fill(flex::Color(0.1f, 0.1f, 0.12f, 1.0f));
    artboard->add_child(bg);

    // Left panel: Album cover
    auto cover_group = flex::ReactiveGroup::create(player_state);
    cover_group->set_position(100, 150);

    cover_group->add_bound_component(
        "AlbumCover",
        {{"size", 300.0f}, {"color1", uint32_t(0xFF8E2DE2)}, {"color2", uint32_t(0xFF4A00E0)}},
        {},
        0, 0
    );

    artboard->add_child(cover_group);

    // Right panel: Player controls
    auto controls = flex::Group::create();
    controls->set_position(450, 150);

    // Song title
    auto title = flex::Text::create();
    title->set_content("Neon Lights");
    title->set_font_size(28);
    title->set_color(flex::Color(1, 1, 1, 1));
    title->set_position(0, 0);
    controls->add_child(title);

    // Artist
    auto artist = flex::Text::create();
    artist->set_content("Synthwave Dreams");
    artist->set_font_size(18);
    artist->set_color(flex::Color(0.7f, 0.7f, 0.7f, 1));
    artist->set_position(0, 40);
    controls->add_child(artist);

    // Seek bar (manually managed - no auto rebuild)
    flex::Node::Ptr seek_bar = flex::create_component_instance(
        "SeekBar",
        flex::Props{{"progress", 0.0f}, {"width", SEEK_BAR_WIDTH}, {"color", uint32_t(0xFF1DB954)}}
    );
    seek_bar->set_position(0, 100);
    setup_seek_bar_events(seek_bar, player_state, SEEK_BAR_WIDTH, SEEK_BAR_ABS_X);
    controls->add_child(seek_bar);

    // Time labels (current / total)
    auto time_label = flex::Text::create();
    time_label->set_content("0:00 / 3:05");
    time_label->set_font_size(14);
    time_label->set_color(flex::Color(0.6f, 0.6f, 0.6f, 1));
    time_label->set_position(0, 125);
    controls->add_child(time_label);

    // Play/Pause button (reactive + interactive)
    auto play_binding = std::make_shared<flex::StateComponentBinding>(
        player_state,
        "PlayButton",
        flex::Props{{"playing", false}, {"color", uint32_t(0xFF1DB954)}}
    );
    play_binding->bind("playing", "playing");

    auto play_button = play_binding->component();
    play_button->set_position(130, 160);

    // Setup click event
    setup_play_button_events(play_button, player_state);

    // Handle rebuild (re-setup events)
    play_binding->on_rebuild([controls, &play_button, player_state](flex::Node::Ptr old_node, flex::Node::Ptr new_node) {
        controls->remove_child(old_node.get());
        new_node->set_position(130, 160);
        controls->insert_child(new_node, 4); // After time label
        play_button = new_node;
        setup_play_button_events(play_button, player_state);
    });

    controls->add_child(play_button);

    // Volume control
    auto volume_label = flex::Text::create();
    volume_label->set_content("Volume");
    volume_label->set_font_size(14);
    volume_label->set_color(flex::Color(0.7f, 0.7f, 0.7f, 1));
    volume_label->set_position(0, 240);
    controls->add_child(volume_label);

    // Volume slider (manually managed - no auto rebuild)
    flex::Node::Ptr volume_slider = flex::create_component_instance(
        "VolumeSlider",
        flex::Props{{"value", 0.7f}, {"width", VOLUME_SLIDER_WIDTH}, {"color", uint32_t(0xFFFFFFFF)}}
    );
    volume_slider->set_position(0, 265);
    setup_volume_slider_events(volume_slider, player_state, VOLUME_SLIDER_WIDTH, VOLUME_SLIDER_ABS_X);
    controls->add_child(volume_slider);

    artboard->add_child(controls);

    // ============================================================================
    // Render Setup
    // ============================================================================

    if (tvg::Initializer::init(0) != tvg::Result::Success) {
        std::cerr << "ThorVG init failed" << std::endl;
        return 1;
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    auto canvas = std::unique_ptr<tvg::SwCanvas>(tvg::SwCanvas::gen());
    canvas->target(
        static_cast<uint32_t*>(surface->pixels),
        surface->pitch / 4,
        surface->w,
        surface->h,
        tvg::ColorSpace::ARGB8888
    );

    auto renderer = flex::create_thorvg_renderer(canvas.get());

    // ============================================================================
    // Event Loop
    // ============================================================================

    bool running = true;
    SDL_Event event;

    Uint32 last_time = SDL_GetTicks();

    std::cout << "=================================================\n";
    std::cout << "Flex Music Player\n";
    std::cout << "=================================================\n";
    std::cout << "Mouse Controls:\n";
    std::cout << "  Click Play Button - Play/Pause\n";
    std::cout << "  Drag Progress Bar - Seek to position\n";
    std::cout << "  Drag Volume Slider - Adjust volume\n\n";
    std::cout << "Keyboard Controls:\n";
    std::cout << "  SPACE - Play/Pause\n";
    std::cout << "  LEFT/RIGHT - Seek ±5 seconds\n";
    std::cout << "  UP/DOWN - Volume ±10%\n";
    std::cout << "  ESC - Exit\n\n";

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                // Send mouse down event to instance
                int mouse_x, mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                instance->send_pointer_event(static_cast<float>(mouse_x), static_cast<float>(mouse_y), true);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                // Send mouse up event to instance
                int mouse_x, mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                instance->send_pointer_event(static_cast<float>(mouse_x), static_cast<float>(mouse_y), false);

                // Clear global drag state (safety fallback)
                g_drag_state.is_seeking = false;
                g_drag_state.is_adjusting_volume = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                // Send mouse move event to instance
                instance->send_pointer_event(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y), event.motion.state & SDL_BUTTON_LMASK);

                // Handle global drag operations (seek bar, volume slider)
                handle_global_mouse_move(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y), player_state);
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_SPACE: {
                        // Toggle play/pause
                        bool is_playing = player_state->get_bool("playing", false);
                        player_state->set("playing", !is_playing);
                        std::cout << (is_playing ? "⏸ Paused" : "▶ Playing") << std::endl;
                        break;
                    }
                    case SDLK_LEFT: {
                        // Seek backward 5 seconds
                        float current = player_state->get_float("current_time", 0);
                        player_state->set("current_time", std::max(0.0f, current - 5.0f));
                        break;
                    }
                    case SDLK_RIGHT: {
                        // Seek forward 5 seconds
                        float current = player_state->get_float("current_time", 0);
                        float duration = player_state->get_float("duration", 185.0f);
                        player_state->set("current_time", std::min(duration, current + 5.0f));
                        break;
                    }
                    case SDLK_UP: {
                        // Volume up
                        float vol = player_state->get_float("volume", 0.7f);
                        player_state->set("volume", std::min(1.0f, vol + 0.1f));
                        break;
                    }
                    case SDLK_DOWN: {
                        // Volume down
                        float vol = player_state->get_float("volume", 0.7f);
                        player_state->set("volume", std::max(0.0f, vol - 0.1f));
                        break;
                    }
                }
            }
        }

        // Update time
        Uint32 current_time_ms = SDL_GetTicks();
        float dt = (current_time_ms - last_time) / 1000.0f;
        last_time = current_time_ms;

        // Update playback time
        if (player_state->get_bool("playing", false)) {
            float current = player_state->get_float("current_time", 0);
            float duration = player_state->get_float("duration", 185.0f);
            current += dt;
            if (current >= duration) current = 0;
            player_state->set("current_time", current);
        }

        // Update progress and seek bar UI
        {
            static float last_progress = -1.0f;
            float current = player_state->get_float("current_time", 0);
            float duration = player_state->get_float("duration", 185.0f);
            float progress = current / duration;

            if (std::abs(progress - last_progress) > 0.001f) {
                auto new_seek_bar = flex::create_component_instance(
                    "SeekBar",
                    flex::Props{{"progress", progress}, {"width", SEEK_BAR_WIDTH}, {"color", uint32_t(0xFF1DB954)}}
                );
                new_seek_bar->set_position(0, 100);
                setup_seek_bar_events(new_seek_bar, player_state, SEEK_BAR_WIDTH, SEEK_BAR_ABS_X);

                // Replace old seek bar
                controls->remove_child(seek_bar.get());
                controls->insert_child(new_seek_bar, 2); // After title and artist
                seek_bar = new_seek_bar;

                // Update time label
                time_label->set_content(format_time(current) + " / " + format_time(duration));
                
                last_progress = progress;
            }
        }

        // Update volume slider UI
        {
            static float last_vol_ui = -1.0f;
            float current_volume = player_state->get_float("volume", 0.7f);
            if (std::abs(current_volume - last_vol_ui) > 0.001f) {
                auto new_volume_slider = flex::create_component_instance(
                    "VolumeSlider",
                    flex::Props{{"value", current_volume}, {"width", VOLUME_SLIDER_WIDTH}, {"color", uint32_t(0xFFFFFFFF)}}
                );
                new_volume_slider->set_position(0, 265);
                setup_volume_slider_events(new_volume_slider, player_state, VOLUME_SLIDER_WIDTH, VOLUME_SLIDER_ABS_X);

                // Replace old volume slider
                controls->remove_child(volume_slider.get());
                controls->add_child(new_volume_slider);
                volume_slider = new_volume_slider;

                last_vol_ui = current_volume;
            }
        }

        // Render
        renderer->begin_frame(800, 600, 1.0f);
        instance->render(*renderer);
        renderer->end_frame();

        canvas->draw();
        canvas->sync();

        SDL_UpdateWindowSurface(window);
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    renderer.reset();
    canvas.reset();
    tvg::Initializer::term();

    SDL_DestroyWindow(window);
    SDL_Quit();
    flex::shutdown();

    return 0;
}
