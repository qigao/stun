/*
 * Flex State Management Demo
 *
 * Demonstrates reactive UI with Observable State:
 * - State changes automatically update components
 * - Multiple components bound to same state
 * - Batch updates for performance
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

#include <SDL.h>
#include <thorvg.h>
#include <iostream>
#include <cmath>
#include <memory>

// ============================================================================
// Component Registration (same as component_dsl_demo)
// ============================================================================

flex::Node::Ptr build_slider(const flex::Props& props) {
    auto slider = flex::Group::create();

    float value = flex::get_prop<float>(props, "value", 0.5f);
    float width = flex::get_prop<float>(props, "width", 300.0f);
    uint32_t color = flex::get_prop<uint32_t>(props, "color", 0xFF0D6EFD);

    // Extract RGBA from uint32_t
    float a = ((color >> 24) & 0xFF) / 255.0f;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    // Background track
    auto track = flex::Shape::create();
    track->set_rect(width, 8);
    track->set_fill(flex::Color(0.9f, 0.9f, 0.9f, 1.0f));
    track->set_y(6);
    slider->add_child(track);

    // Filled portion
    auto fill = flex::Shape::create();
    fill->set_rect(width * value, 8);
    fill->set_fill(flex::Color(r, g, b, a));
    fill->set_y(6);
    slider->add_child(fill);

    // Thumb (circle)
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

    // Background
    auto bg = flex::Shape::create();
    bg->set_rect(width, 20);
    bg->set_fill(flex::Color(0.9f, 0.9f, 0.9f, 1.0f));
    bar->add_child(bg);

    // Progress fill
    auto fill = flex::Shape::create();
    fill->set_rect(width * progress, 20);
    fill->set_fill(flex::Color(r, g, b, a));
    bar->add_child(fill);

    return bar;
}

void register_components() {
    // Slider component
    auto slider_comp = flex::Component::create("Slider");
    slider_comp->add_prop("value", 0.5f);
    slider_comp->add_prop("width", 300.0f);
    slider_comp->add_prop("color", uint32_t(0xFF0D6EFD));
    slider_comp->set_builder(build_slider);
    flex::ComponentRegistry::instance().register_component(slider_comp);

    // ProgressBar component
    auto progress_comp = flex::Component::create("ProgressBar");
    progress_comp->add_prop("progress", 0.5f);
    progress_comp->add_prop("width", 300.0f);
    progress_comp->add_prop("color", uint32_t(0xFF198754));
    progress_comp->set_builder(build_progress_bar);
    flex::ComponentRegistry::instance().register_component(progress_comp);
}

// ============================================================================
// Demo Application
// ============================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // Initialize Flex and SDL
    flex::init();
    // Load font
    if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
        flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Flex State Management Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1200, 800,
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
    // Create Observable State
    // ============================================================================

    auto app_state = flex::ObservableState::create();

    // Initialize state values
    app_state->set("cpu_usage", 0.3f);
    app_state->set("memory_usage", 0.65f);
    app_state->set("disk_usage", 0.82f);
    app_state->set("network_speed", 0.45f);

    // ============================================================================
    // Build UI with State Bindings
    // ============================================================================

    auto instance = flex::Instance::create(1200, 800);
    auto artboard = instance->artboard();

    // Background
    auto bg = flex::Shape::create();
    bg->set_rect(1200, 800);
    bg->set_fill(flex::Color(0.97f, 0.97f, 0.98f, 1.0f));
    artboard->add_child(bg);

    // Title
    auto title = flex::Text::create();
    title->set_content("Reactive State Management Demo");
    title->set_font_size(32);
    title->set_position(50, 40);
    artboard->add_child(title);

    // Subtitle
    auto subtitle = flex::Text::create();
    subtitle->set_content("State changes automatically update all bound components");
    subtitle->set_font_size(16);
    subtitle->set_color(flex::Color(0.5f, 0.5f, 0.5f, 1.0f));
    subtitle->set_position(50, 80);
    artboard->add_child(subtitle);

    // ============================================================================
    // Reactive Group 1: CPU Usage
    // ============================================================================

    auto cpu_group = flex::ReactiveGroup::create(app_state);
    cpu_group->set_position(50, 140);

    auto cpu_label = flex::Text::create();
    cpu_label->set_content("CPU Usage");
    cpu_label->set_font_size(18);
    cpu_group->add_child(cpu_label);

    // Slider bound to cpu_usage state
    cpu_group->add_bound_component(
        "Slider",
        {{"value", 0.3f}, {"width", 400.0f}, {"color", uint32_t(0xFF0D6EFD)}},
        {{"cpu_usage", "value"}},  // state key -> prop name
        0, 40
    );

    // Progress bar also bound to cpu_usage state
    cpu_group->add_bound_component(
        "ProgressBar",
        {{"progress", 0.3f}, {"width", 400.0f}, {"color", uint32_t(0xFF0D6EFD)}},
        {{"cpu_usage", "progress"}},
        0, 90
    );

    artboard->add_child(cpu_group);

    // ============================================================================
    // Reactive Group 2: Memory Usage
    // ============================================================================

    auto mem_group = flex::ReactiveGroup::create(app_state);
    mem_group->set_position(50, 290);

    auto mem_label = flex::Text::create();
    mem_label->set_content("Memory Usage");
    mem_label->set_font_size(18);
    mem_group->add_child(mem_label);

    mem_group->add_bound_component(
        "Slider",
        {{"value", 0.65f}, {"width", 400.0f}, {"color", uint32_t(0xFFDC3545)}},
        {{"memory_usage", "value"}},
        0, 40
    );

    mem_group->add_bound_component(
        "ProgressBar",
        {{"progress", 0.65f}, {"width", 400.0f}, {"color", uint32_t(0xFFDC3545)}},
        {{"memory_usage", "progress"}},
        0, 90
    );

    artboard->add_child(mem_group);

    // ============================================================================
    // Reactive Group 3: Disk Usage
    // ============================================================================

    auto disk_group = flex::ReactiveGroup::create(app_state);
    disk_group->set_position(50, 440);

    auto disk_label = flex::Text::create();
    disk_label->set_content("Disk Usage");
    disk_label->set_font_size(18);
    disk_group->add_child(disk_label);

    disk_group->add_bound_component(
        "Slider",
        {{"value", 0.82f}, {"width", 400.0f}, {"color", uint32_t(0xFFFFC107)}},
        {{"disk_usage", "value"}},
        0, 40
    );

    disk_group->add_bound_component(
        "ProgressBar",
        {{"progress", 0.82f}, {"width", 400.0f}, {"color", uint32_t(0xFFFFC107)}},
        {{"disk_usage", "progress"}},
        0, 90
    );

    artboard->add_child(disk_group);

    // ============================================================================
    // Reactive Group 4: Network Speed
    // ============================================================================

    auto net_group = flex::ReactiveGroup::create(app_state);
    net_group->set_position(50, 590);

    auto net_label = flex::Text::create();
    net_label->set_content("Network Speed");
    net_label->set_font_size(18);
    net_group->add_child(net_label);

    net_group->add_bound_component(
        "Slider",
        {{"value", 0.45f}, {"width", 400.0f}, {"color", uint32_t(0xFF198754)}},
        {{"network_speed", "value"}},
        0, 40
    );

    net_group->add_bound_component(
        "ProgressBar",
        {{"progress", 0.45f}, {"width", 400.0f}, {"color", uint32_t(0xFF198754)}},
        {{"network_speed", "progress"}},
        0, 90
    );

    artboard->add_child(net_group);

    // ============================================================================
    // Info Panel (right side)
    // ============================================================================

    auto info = flex::Text::create();
    info->set_content(
        "Watch the UI update automatically!\n\n"
        "State changes every 2 seconds:\n"
        "- Both slider and progress bar update\n"
        "- Multiple components bound to same state\n"
        "- Zero manual UI updates needed\n\n"
        "This is reactive programming!"
    );
    info->set_font_size(14);
    info->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
    info->set_position(550, 140);
    artboard->add_child(info);

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
    // Animation Loop - Simulate State Changes
    // ============================================================================

    bool running = true;
    SDL_Event event;

    float time = 0;
    Uint32 last_time = SDL_GetTicks();

    std::cout << "=================================================\n";
    std::cout << "Flex State Management Demo\n";
    std::cout << "=================================================\n";
    std::cout << "Watch components update automatically as state changes!\n";
    std::cout << "Press ESC or close window to exit.\n\n";

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        Uint32 current_time = SDL_GetTicks();
        float dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        time += dt;

        // Update state values with sine waves (simulate changing metrics)
        // State changes automatically trigger component rebuilds!
        app_state->begin_batch();  // Batch updates for performance

        app_state->set("cpu_usage", 0.5f + 0.3f * std::sin(time * 0.8f));
        app_state->set("memory_usage", 0.6f + 0.2f * std::sin(time * 1.2f));
        app_state->set("disk_usage", 0.75f + 0.15f * std::sin(time * 0.5f));
        app_state->set("network_speed", 0.4f + 0.35f * std::sin(time * 1.5f));

        app_state->end_batch();  // Trigger all updates at once

        // Render
        renderer->begin_frame(1200, 800, 1.0f);
        instance->render(*renderer);
        renderer->end_frame();

        canvas->draw();
        canvas->sync();

        SDL_UpdateWindowSurface(window);

        SDL_Delay(16);  // ~60 FPS
    }

    std::cout << "\nDemo finished. Total runtime: " << time << " seconds\n";

    renderer.reset();
    canvas.reset();
    tvg::Initializer::term();

    SDL_DestroyWindow(window);
    SDL_Quit();
    flex::shutdown();

    return 0;
}
