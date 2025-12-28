/*
 * Meta Editor - Demo Application
 * 
 * Demonstrates basic meta editor functionality.
 */

#include "meta_editor/meta_editor.h"
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/bridge/renderer.h>
#include <flex/backends/thorvg/init.h>
#include <fstream>
#include <vector>
#include <iostream>
bool load_font(const char* name, const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open font file: " << path << std::endl;
        return false;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        return false;
    }

    if (tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) != tvg::Result::Success) {
        return false;
    }

    std::cout << "Font loaded: " << name << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Create window
    const int WINDOW_WIDTH = 1200;
    const int WINDOW_HEIGHT = 800;
    
    std::cout << "Creating window " << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << "..." << std::endl;
    
    SDL_Window* window = SDL_CreateWindow(
        "Meta Editor Demo",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface) {
        std::cerr << "Failed to get window surface: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    try {
        // Initialize ThorVG
        if (tvg::Initializer::init(1) != tvg::Result::Success) { // Use at least 1 thread
            throw std::runtime_error("ThorVG initialization failed");
        }

        // Load fonts
        load_font("Arial", "C:/Windows/Fonts/arial.ttf");

        // Initialize flex (required for logging/backends)
        std::cout << "Initializing Flex engine..." << std::endl;
        flex::init();

        // Create ThorVG SwCanvas
        std::unique_ptr<tvg::SwCanvas> tvg_canvas(tvg::SwCanvas::gen());
        tvg_canvas->target(
            reinterpret_cast<uint32_t*>(surface->pixels),
            surface->w,
            surface->pitch / 4,
            surface->h,
            tvg::ColorSpace::ARGB8888
        );

        // Create flex renderer
        auto renderer = flex::create_thorvg_renderer(tvg_canvas.get());
        if (!renderer) {
            throw std::runtime_error("Failed to create flex renderer");
        }

        // Create meta editor
        std::cout << "Initializing Meta Editor..." << std::endl;
        meta_editor::MetaEditor editor((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
        editor.init(renderer.get());
        
        std::cout << "Meta Editor Demo started!" << std::endl;
        
        // Event loop state
        bool running = true;
        Uint32 last_time = SDL_GetTicks();
        
        while (running) {
            // Handle events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Handle window resize
                    int new_width = event.window.data1;
                    int new_height = event.window.data2;

                    // Update surface
                    surface = SDL_GetWindowSurface(window);
                    if (surface) {
                        // Re-target the canvas
                        tvg_canvas->target(
                            reinterpret_cast<uint32_t*>(surface->pixels),
                            surface->w,
                            surface->pitch / 4,
                            surface->h,
                            tvg::ColorSpace::ARGB8888
                        );

                        // Update editor viewport
                        editor.set_viewport((float)new_width, (float)new_height);
                    }
                } else {
                    // Let editor handle event
                    // If editor handles it, it won't propagate (e.g. to tool-specific logic)
                    editor.handle_event(event);
                }
            }
            
            // Update & Render Flow
            // 1. Calculate delta time
            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;
            
            // 2. Update editor
            // This now includes UI update, layout and rendering because of WorkspaceWidget
            editor.update(dt);
            
            // 3. Update window surface
            // tvg_canvas handles draw() and sync() inside flex::Renderer::end_frame()
            // which is called by ui_box_->update().
            SDL_UpdateWindowSurface(window);
            
            // Cap frame rate
            SDL_Delay(16);  // ~60 FPS
        }
        
        // Cleanup
        std::cout << "Shutting down Meta Editor..." << std::endl;
        editor.shutdown();
        flex::shutdown();
        tvg::Initializer::term();
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "FATAL ERROR: Unknown exception" << std::endl;
    }
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
