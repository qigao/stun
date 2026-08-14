/*
 * flexUI Designer - Main Application
 */

#include "flexui_designer/designer.h"
#include "flexui_designer/property_editor.h"
#include "backends/opengl/init.h"
#include <flex/bridge/renderer.h>
#include <glad/glad.h>
#include <SDL2/SDL.h>
#include <iostream>

using namespace flexui_designer;

int main(int argc, char* argv[]) {
    int width = 1280, height = 720;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("flexUI Designer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context || SDL_GL_MakeCurrent(window, gl_context) != 0) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << "\n";
        if (gl_context) SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        std::cerr << "GLAD init failed\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    flex::opengl_backend::init();
    flex::opengl_backend::register_backend();
    flex::opengl_backend::load_font("sans", "C:/Windows/Fonts/arial.ttf");
    flex::opengl_backend::OpenGLCanvas canvas;
    canvas.get_proc_address = [](void*, const char* name) {
        return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(
            SDL_GL_GetProcAddress(name));
    };
    auto renderer = flex::opengl_backend::create_renderer(&canvas);
    if (!renderer) {
        std::cerr << "gCanvas renderer creation failed\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

        int drawable_width = 0;
        int drawable_height = 0;
        SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
        if (width <= 0 || height <= 0 || drawable_width <= 0 || drawable_height <= 0) {
            SDL_Delay(16);
            continue;
        }
        const float pixel_ratio = static_cast<float>(drawable_width) /
                                  static_cast<float>(width);
        glViewport(0, 0, drawable_width, drawable_height);
        renderer->begin_frame((float)width, (float)height, pixel_ratio);
        renderer->clear(flex::Color{0.1f, 0.1f, 0.12f, 1.0f});
        designer.render(*renderer);
        renderer->end_frame();

        SDL_GL_SwapWindow(window);
    }

    designer.shutdown();
    renderer.reset();
    flex::opengl_backend::shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
