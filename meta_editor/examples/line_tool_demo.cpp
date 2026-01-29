/*
 * Meta Editor - Line Tool Demo
 *
 * Demonstrates:
 * 1. Registering a custom tool (LineTool)
 * 2. Creating a custom panel (LineStylePanel)
 * 3. Integrating them with the Editor SDK
 *
 * Build: cmake --build build --target line_tool_demo
 * Run:   ./line_tool_demo
 *
 * Controls:
 *   L - Line tool
 *   V - Select tool
 *   Shift - Constrain to 45° angles
 *   Middle mouse - Pan
 *   Scroll - Zoom
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/sdl_adapter.h>
#include <meta_editor/tools/line_tool.h>
#include <meta_editor/view/line_style_panel.h>
#include <flex/backends/thorvg/init.h>
#include <flex/bridge/renderer.h>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <iostream>
#include <memory>

using namespace meta_editor;

int main(int argc, char* argv[]) {
    const int WIDTH = 1024;
    const int HEIGHT = 768;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Line Tool Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Surface* surface = SDL_GetWindowSurface(window);

    flex::init();
    flex::load_font("Arial", "C:/Windows/Fonts/arial.ttf");

    std::unique_ptr<tvg::SwCanvas> tvg_canvas(tvg::SwCanvas::gen());
    tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                       surface->w, surface->pitch / 4, surface->h,
                       tvg::ColorSpace::ARGB8888);

    auto renderer = flex::create_thorvg_renderer(tvg_canvas.get());

    // Create Editor
    Editor editor((float)WIDTH, (float)HEIGHT);
    editor.init();

    // Register custom LineTool
    auto line_tool = std::make_unique<LineTool>();
    LineTool* line_tool_ptr = line_tool.get();
    editor.tools()->register_tool(std::move(line_tool));

    // Create LineStylePanel
    LineStylePanel style_panel(editor.selection(), line_tool_ptr);
    style_panel.set_position(16, 200);

    std::cout << "Line Tool Demo\n";
    std::cout << "==============\n";
    std::cout << "L - Line tool\n";
    std::cout << "V - Select tool\n";
    std::cout << "Shift - Constrain to 45 degree angles\n";
    std::cout << "Click color swatches to change stroke color\n";
    std::cout << "Click width buttons to change stroke width\n\n";

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

            // Handle panel clicks first
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (style_panel.handle_click((float)e.button.x, (float)e.button.y)) {
                    continue;
                }
            }

            // Panning
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
                editor.canvas()->pan((float)(e.motion.x - last_mx), (float)(e.motion.y - last_my));
                last_mx = e.motion.x;
                last_my = e.motion.y;
                continue;
            }

            // Zoom
            if (e.type == SDL_MOUSEWHEEL) {
                editor.canvas()->zoom_at((float)last_mx, (float)last_my, 1.0f + e.wheel.y * 0.1f);
                continue;
            }

            if (e.type == SDL_MOUSEMOTION) {
                last_mx = e.motion.x;
                last_my = e.motion.y;
            }

            // Tool switching
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_l) {
                    editor.tools()->set_active_tool("Line");
                    continue;
                }
                if (e.key.keysym.sym == SDLK_v) {
                    editor.tools()->set_active_tool("Select");
                    continue;
                }
            }

            // Pass to editor
            EditorEvent ev = sdl_to_editor_event(e);
            if (ev.type != EditorEvent::Type::None) {
                editor.handle_event(ev);
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last_time) / 1000.0f;
        last_time = now;
        editor.update(dt);

        renderer->begin_frame((float)surface->w, (float)surface->h, 1.0f);
        renderer->clear(flex::Color{0.12f, 0.12f, 0.14f, 1.0f});
        editor.render(*renderer);
        editor.render_tool_overlay(*renderer);
        style_panel.render(*renderer);
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
