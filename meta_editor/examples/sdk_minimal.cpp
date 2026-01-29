/*
 * Meta Editor SDK - Minimal Example
 *
 * Demonstrates using Editor directly without MetaEditor's UI panels.
 * This is the "headless" approach for embedding in custom applications.
 *
 * Build: cmake --build build --target sdk_minimal_demo
 * Run:   ./sdk_minimal_demo
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/sdl_adapter.h>  // SDL → EditorEvent conversion
#include <flex/backends/thorvg/init.h>
#include <flex/bridge/renderer.h>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <iostream>

using namespace meta_editor;

int main(int argc, char* argv[]) {
    const int WIDTH = 1024;
    const int HEIGHT = 768;

    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Meta Editor SDK Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Surface* surface = SDL_GetWindowSurface(window);

    // Initialize flex engine
    flex::init();
    flex::load_font("Arial", "C:/Windows/Fonts/arial.ttf");

    // Create ThorVG SwCanvas (CPU rendering - simple but slower)
    std::unique_ptr<tvg::SwCanvas> tvg_canvas(tvg::SwCanvas::gen());
    tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                       surface->w, surface->pitch / 4, surface->h,
                       tvg::ColorSpace::ARGB8888);

    // Create flex renderer
    auto renderer = flex::create_thorvg_renderer(tvg_canvas.get());

    // Create Editor (SDK core - no UI panels)
    Editor editor((float)WIDTH, (float)HEIGHT);
    editor.init();

    // Add some shapes programmatically
    auto* canvas = editor.canvas();
    auto* allocator = canvas->instance()->object_allocator();
    auto layers = canvas->get_all_layers();

    if (!layers.empty()) {
        auto* layer = layers[0];

        // Create a rectangle
        auto* rect = flex::Shape::create(*allocator);
        rect->set_rect(100, 80, 8);
        rect->set_position(100, 100);
        rect->set_fill(flex::Color{0.2f, 0.6f, 0.9f, 1.0f});
        rect->set_stroke(flex::Color{0.1f, 0.3f, 0.5f, 1.0f}, 2.0f);
        layer->add_child(rect);

        // Create a circle
        auto* circle = flex::Shape::create(*allocator);
        circle->set_circle(50);
        circle->set_position(350, 150);
        circle->set_fill(flex::Color{0.9f, 0.3f, 0.3f, 1.0f});
        layer->add_child(circle);

        // Create a star
        auto* star = flex::Shape::create(*allocator);
        star->set_star(5, 60, 30);
        star->set_position(550, 150);
        star->set_fill(flex::Color{0.9f, 0.8f, 0.2f, 1.0f});
        star->set_stroke(flex::Color{0.6f, 0.5f, 0.1f, 1.0f}, 2.0f);
        layer->add_child(star);
    }

    std::cout << "Meta Editor SDK Demo\n";
    std::cout << "====================\n";
    std::cout << "V - Select tool\n";
    std::cout << "R - Rectangle tool\n";
    std::cout << "O - Circle tool\n";
    std::cout << "P - Pen tool\n";
    std::cout << "Delete - Delete selection\n";
    std::cout << "Ctrl+Z - Undo\n";
    std::cout << "Ctrl+Y - Redo\n";
    std::cout << "Ctrl+G - Group\n";
    std::cout << "Ctrl+Shift+G - Ungroup\n";
    std::cout << "Middle mouse - Pan\n";
    std::cout << "Scroll - Zoom\n\n";

    // Main loop
    bool running = true;
    bool panning = false;
    int last_mx = 0, last_my = 0;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (is_quit_event(e)) {
                running = false;
                break;
            }

            if (is_window_resize(e)) {
                surface = SDL_GetWindowSurface(window);
                tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                                   surface->w, surface->pitch / 4, surface->h,
                                   tvg::ColorSpace::ARGB8888);
                renderer = flex::create_thorvg_renderer(tvg_canvas.get());
                editor.set_viewport((float)surface->w, (float)surface->h);
                continue;
            }

            // Handle panning with middle mouse
            if (is_middle_button_down(e)) {
                panning = true;
                last_mx = e.button.x;
                last_my = e.button.y;
                continue;
            }
            if (is_middle_button_up(e)) {
                panning = false;
                continue;
            }
            if (e.type == SDL_MOUSEMOTION && panning) {
                int dx = e.motion.x - last_mx;
                int dy = e.motion.y - last_my;
                canvas->pan((float)dx, (float)dy);
                last_mx = e.motion.x;
                last_my = e.motion.y;
                continue;
            }

            // Handle zoom with scroll wheel
            if (e.type == SDL_MOUSEWHEEL) {
                canvas->zoom_at((float)last_mx, (float)last_my, 1.0f + e.wheel.y * 0.1f);
                continue;
            }

            // Track mouse position for zoom
            if (e.type == SDL_MOUSEMOTION) {
                last_mx = e.motion.x;
                last_my = e.motion.y;
            }

            // Convert and dispatch to Editor
            EditorEvent ev = sdl_to_editor_event(e);
            if (ev.type != EditorEvent::Type::None) {
                editor.handle_event(ev);
            }
        }

        // Update
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_time) / 1000.0f;
        last_time = now;
        editor.update(dt);

        // Render
        renderer->begin_frame((float)surface->w, (float)surface->h, 1.0f);
        renderer->clear(flex::Color{0.12f, 0.12f, 0.14f, 1.0f});
        editor.render(*renderer);
        editor.render_tool_overlay(*renderer);
        renderer->end_frame();

        SDL_UpdateWindowSurface(window);
        SDL_Delay(16);
    }

    editor.shutdown();
    flex::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
