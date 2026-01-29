/*
 * SVG Editor Demo - Inkscape-like SVG editor
 *
 * Features:
 * - Import SVG files (drag & drop or Ctrl+O)
 * - Edit shapes (select, move, resize, rotate)
 * - Draw new shapes (pen, rectangle, circle, etc.)
 * - Export to SVG (Ctrl+S)
 *
 * Usage:
 *   ./svg_editor_demo [input.svg]
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/sdl_adapter.h>
#include <meta_editor/view/tool_panel.h>
#include <meta_editor/view/properties_panel.h>
#include <meta_editor/view/layers_panel.h>
#include <flex/backends/thorvg/init.h>
#include <flex/bridge/renderer.h>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

using namespace meta_editor;

static std::string open_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "SVG Files\0*.svg\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) return filename;
#endif
    return "";
}

static std::string save_file_dialog(const std::string& current) {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    if (!current.empty()) strncpy(filename, current.c_str(), MAX_PATH - 1);

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "SVG Files\0*.svg\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "svg";
    if (GetSaveFileNameA(&ofn)) return filename;
#endif
    return "";
}

int main(int argc, char* argv[]) {
    int width = 1280, height = 800;
    std::string current_file;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SVG Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Surface* surface = SDL_GetWindowSurface(window);

    flex::init();
    flex::load_font("Arial", "C:/Windows/Fonts/arial.ttf");

    std::unique_ptr<tvg::SwCanvas> tvg_canvas(tvg::SwCanvas::gen());
    tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                       surface->w, surface->pitch / 4, surface->h,
                       tvg::ColorSpace::ARGB8888);

    auto renderer = flex::create_thorvg_renderer(tvg_canvas.get());

    Editor editor((float)width, (float)height);
    editor.init();

    // Panels
    ToolPanel tool_panel(editor.tools());
    tool_panel.set_position(16, 16);

    PropertiesPanel props_panel(editor.canvas(), editor.selection());
    props_panel.set_position((float)width - 220, 16);

    LayersPanel layers_panel(editor.canvas(), editor.selection());
    layers_panel.set_position((float)width - 220, 300);

    // Load initial file if provided
    if (argc > 1) {
        if (editor.import_svg(argv[1])) {
            current_file = argv[1];
            std::string title = "SVG Editor - " + current_file;
            SDL_SetWindowTitle(window, title.c_str());
        }
    }

    std::cout << "SVG Editor\n";
    std::cout << "==========\n";
    std::cout << "Ctrl+O - Open SVG\n";
    std::cout << "Ctrl+S - Save SVG\n";
    std::cout << "Ctrl+N - New document\n";
    std::cout << "Drag & drop SVG files to import\n\n";

    bool running = true;
    bool panning = false;
    int last_mx = 0, last_my = 0;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }

            // Window resize
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                width = e.window.data1;
                height = e.window.data2;
                surface = SDL_GetWindowSurface(window);
                tvg_canvas->target(reinterpret_cast<uint32_t*>(surface->pixels),
                                   surface->w, surface->pitch / 4, surface->h,
                                   tvg::ColorSpace::ARGB8888);
                renderer = flex::create_thorvg_renderer(tvg_canvas.get());
                editor.set_viewport((float)width, (float)height);
                props_panel.set_position((float)width - 220, 16);
                layers_panel.set_position((float)width - 220, 300);
                continue;
            }

            // Drag & drop
            if (e.type == SDL_DROPFILE) {
                if (editor.import_svg(e.drop.file)) {
                    current_file = e.drop.file;
                    std::string title = "SVG Editor - " + current_file;
                    SDL_SetWindowTitle(window, title.c_str());
                }
                SDL_free(e.drop.file);
                continue;
            }

            // Keyboard shortcuts
            if (e.type == SDL_KEYDOWN) {
                bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
                if (ctrl && e.key.keysym.sym == SDLK_o) {
                    auto file = open_file_dialog();
                    if (!file.empty() && editor.import_svg(file)) {
                        current_file = file;
                        std::string title = "SVG Editor - " + current_file;
                        SDL_SetWindowTitle(window, title.c_str());
                    }
                    continue;
                }
                if (ctrl && e.key.keysym.sym == SDLK_s) {
                    auto file = save_file_dialog(current_file);
                    if (!file.empty()) {
                        editor.save_svg(file);
                        current_file = file;
                        std::string title = "SVG Editor - " + current_file;
                        SDL_SetWindowTitle(window, title.c_str());
                    }
                    continue;
                }
                if (ctrl && e.key.keysym.sym == SDLK_n) {
                    editor.shutdown();
                    editor.init();
                    current_file.clear();
                    SDL_SetWindowTitle(window, "SVG Editor");
                    continue;
                }
            }

            // Middle mouse pan
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_MIDDLE) {
                panning = true;
                last_mx = e.button.x;
                last_my = e.button.y;
                continue;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_MIDDLE) {
                panning = false;
                continue;
            }
            if (e.type == SDL_MOUSEMOTION && panning) {
                editor.canvas()->pan((float)(e.motion.x - last_mx), (float)(e.motion.y - last_my));
                last_mx = e.motion.x;
                last_my = e.motion.y;
                continue;
            }

            // Scroll zoom
            if (e.type == SDL_MOUSEWHEEL) {
                editor.canvas()->zoom_at((float)last_mx, (float)last_my, 1.0f + e.wheel.y * 0.1f);
                continue;
            }

            if (e.type == SDL_MOUSEMOTION) {
                last_mx = e.motion.x;
                last_my = e.motion.y;
            }

            // Panel clicks
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                float mx = (float)e.button.x, my = (float)e.button.y;
                if (tool_panel.contains(mx, my)) {
                    tool_panel.handle_click(mx, my);
                    continue;
                }
                if (props_panel.contains(mx, my)) {
                    props_panel.handle_click(mx, my);
                    continue;
                }
                if (layers_panel.contains(mx, my)) {
                    layers_panel.handle_click(mx, my);
                    continue;
                }
            }

            // Editor events
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
        renderer->clear(flex::Color{0.15f, 0.15f, 0.17f, 1.0f});
        editor.render(*renderer);
        editor.render_tool_overlay(*renderer);

        tool_panel.render(*renderer);
        props_panel.render(*renderer);
        layers_panel.render(*renderer);

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
