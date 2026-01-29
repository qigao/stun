/*
 * flexUI Designer - Main Application
 */

#include "flexui_designer/designer.h"
#include "flexui_designer/property_editor.h"
#include <flex/backends/thorvg/init.h>
#include <flex/bridge/renderer.h>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <iostream>

using namespace flexui_designer;

int main(int argc, char* argv[]) {
    int width = 1280, height = 720;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("flexUI Designer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Surface* surface = SDL_GetWindowSurface(window);

    flex::init();
    flex::load_font("sans", "C:/Windows/Fonts/arial.ttf");

    std::unique_ptr<tvg::SwCanvas> tvg_canvas(tvg::SwCanvas::gen());
    tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                       surface->w, surface->pitch / 4, surface->h,
                       tvg::ColorSpace::ARGB8888);

    auto renderer = flex::create_thorvg_renderer(tvg_canvas.get());

    Designer designer((float)width, (float)height);
    designer.init(renderer.get());

    std::cout << "flexUI Designer\n";
    std::cout << "===============\n";
    std::cout << "Drag widgets from palette to canvas\n";
    std::cout << "Click to select, drag to move/resize\n";
    std::cout << "Drag empty area to box-select\n";
    std::cout << "Right-click for context menu\n";
    std::cout << "Arrow keys - move (Ctrl for 1px)\n";
    std::cout << "Delete - remove | Ctrl+A - select all\n";
    std::cout << "Ctrl+C/V/D - copy/paste/duplicate\n";
    std::cout << "Ctrl+Z/Y - undo/redo\n";
    std::cout << "G - grid | S - snap | R - rulers | T - tree\n";
    std::cout << "P - preview mode\n";
    std::cout << "Ctrl+S - generate code\n";
    std::cout << "Ctrl+Shift+S - save | Ctrl+O - open\n\n";

    bool running = true;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                width = e.window.data1;
                height = e.window.data2;
                surface = SDL_GetWindowSurface(window);
                tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                                   surface->w, surface->pitch / 4, surface->h,
                                   tvg::ColorSpace::ARGB8888);
                renderer = flex::create_thorvg_renderer(tvg_canvas.get());
                continue;
            }

            if (e.type == SDL_KEYDOWN) {
                bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
                bool shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
                
                if (ctrl && e.key.keysym.sym == SDLK_s && !shift) {
                    std::string code = designer.generate_code();
                    std::cout << "=== Generated Code ===\n" << code << "\n";
                    designer.save_code("generated_ui.cpp");
                    std::cout << "Saved to generated_ui.cpp\n";
                    continue;
                }
                if (ctrl && shift && e.key.keysym.sym == SDLK_s) {
                    designer.save_project("project.fuid");
                    std::cout << "Project saved to project.fuid\n";
                    continue;
                }
                if (ctrl && e.key.keysym.sym == SDLK_o) {
                    if (designer.load_project("project.fuid")) {
                        std::cout << "Project loaded from project.fuid\n";
                    }
                    continue;
                }
                if (ctrl && e.key.keysym.sym == SDLK_n) {
                    designer.new_project();
                    continue;
                }
                if (e.key.keysym.sym == SDLK_p && !ctrl) {
                    designer.toggle_preview();
                    std::cout << (designer.is_preview_mode() ? "Preview mode ON" : "Preview mode OFF") << "\n";
                    continue;
                }
                
                // Handle special keys for property editor
                if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_ESCAPE ||
                    e.key.keysym.sym == SDLK_BACKSPACE) {
                    int key = (e.key.keysym.sym == SDLK_RETURN) ? '\r' :
                              (e.key.keysym.sym == SDLK_ESCAPE) ? 27 : '\b';
                    if (designer.properties()->handle_key(key)) continue;
                }
                
                // Forward all key events to designer (including arrow keys)
                meta_editor::EditorEvent ev;
                ev.type = meta_editor::EditorEvent::Type::KeyDown;
                // Map SDL keys to Windows virtual key codes for arrow keys
                if (e.key.keysym.sym == SDLK_LEFT) ev.key = 0x25;
                else if (e.key.keysym.sym == SDLK_UP) ev.key = 0x26;
                else if (e.key.keysym.sym == SDLK_RIGHT) ev.key = 0x27;
                else if (e.key.keysym.sym == SDLK_DOWN) ev.key = 0x28;
                else ev.key = e.key.keysym.sym;
                ev.mods = 0;
                if (ctrl) ev.mods |= meta_editor::Mod_Ctrl;
                if (e.key.keysym.mod & KMOD_SHIFT) ev.mods |= meta_editor::Mod_Shift;
                if (e.key.keysym.mod & KMOD_ALT) ev.mods |= meta_editor::Mod_Alt;
                designer.handle_event(ev);
                continue;
            }

            if (e.type == SDL_TEXTINPUT) {
                designer.properties()->handle_text_input(e.text.text);
                continue;
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                meta_editor::EditorEvent ev;
                ev.type = meta_editor::EditorEvent::Type::PointerDown;
                ev.x = (float)e.button.x;
                ev.y = (float)e.button.y;
                ev.button = meta_editor::MouseButton::Left;
                designer.handle_event(ev);
                continue;
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
                meta_editor::EditorEvent ev;
                ev.type = meta_editor::EditorEvent::Type::PointerDown;
                ev.x = (float)e.button.x;
                ev.y = (float)e.button.y;
                ev.button = meta_editor::MouseButton::Right;
                designer.handle_event(ev);
                continue;
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                meta_editor::EditorEvent ev;
                ev.type = meta_editor::EditorEvent::Type::PointerUp;
                ev.x = (float)e.button.x;
                ev.y = (float)e.button.y;
                designer.handle_event(ev);
                continue;
            }

            if (e.type == SDL_MOUSEMOTION) {
                meta_editor::EditorEvent ev;
                ev.type = meta_editor::EditorEvent::Type::PointerMove;
                ev.x = (float)e.motion.x;
                ev.y = (float)e.motion.y;
                designer.handle_event(ev);
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last_time) / 1000.0f;
        last_time = now;
        designer.update(dt);

        renderer->begin_frame((float)surface->w, (float)surface->h, 1.0f);
        renderer->clear(flex::Color{0.1f, 0.1f, 0.12f, 1.0f});
        designer.render(*renderer);
        renderer->end_frame();

        SDL_UpdateWindowSurface(window);
        SDL_Delay(16);
    }

    designer.shutdown();
    flex::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
