/*
 * NanoVG Rough - Excalidraw-style Hand-drawn Circles Demo
 *
 * Demonstrates various hand-drawn circle styles similar to Excalidraw
 */

#include <SDL3/SDL.h>

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include "nanovg_rough.h"
#include <iostream>

int main() {
    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("Rough Circles Demo - Excalidraw Style", 1200, 800,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    gladLoadGL();

    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        std::cerr << "Failed to create NanoVG context" << std::endl;
        return -1;
    }

    nvgCreateFont(vg, "sans", "resources/Roboto-Regular.ttf");

    std::cout << "Rough Circles Demo - Excalidraw Style" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Showing various hand-drawn circle styles" << std::endl;
    std::cout << "Press ESC to exit" << std::endl;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);

        // Title
        nvgFontSize(vg, 32.0f);
        nvgFontFace(vg, "sans");
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgText(vg, win_w / 2, 20, "Excalidraw-Style Hand-Drawn Circles", nullptr);

        float y_start = 100;
        float x_spacing = 200;
        float y_spacing = 250;

        // Row 1: Different roughness levels
        nvgFontSize(vg, 16.0f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        
        // Clean (low roughness)
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 0.3f;
            opts.bowing = 0.5f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Clean", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "roughness: 0.3", nullptr);
        }

        // Medium roughness (Excalidraw default)
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150 + x_spacing, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Medium", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "roughness: 1.0", nullptr);
        }

        // Sketchy (high roughness)
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 2.0f;
            opts.bowing = 1.5f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150 + x_spacing * 2, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Sketchy", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "roughness: 2.0", nullptr);
        }

        // Very rough
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 3.5f;
            opts.bowing = 2.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150 + x_spacing * 3, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Very Rough", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "roughness: 3.5", nullptr);
        }

        // Cartoon style
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.2f;
            opts.bowing = 0.8f;
            opts.stroke_count = 2;
            opts.stroke_width = 3.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150 + x_spacing * 4, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Cartoon", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "2 strokes, thick", nullptr);
        }

        // Row 2: Filled circles with hachure
        y_start += y_spacing;

        // Solid fill with hachure
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            opts.fill_enabled = 1;
            opts.fill_color = nvgRGBA(100, 150, 255, 255);
            
            float cx = 150, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Blue Fill", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "hachure pattern", nullptr);
        }

        // Green fill
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.2f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 100, 0, 255);
            opts.fill_enabled = 1;
            opts.fill_color = nvgRGBA(100, 255, 100, 255);
            opts.seed = 42;
            
            float cx = 150 + x_spacing, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Green Fill", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "custom seed", nullptr);
        }

        // Red fill
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.5f;
            opts.bowing = 1.2f;
            opts.stroke_count = 2;
            opts.stroke_width = 2.5f;
            opts.stroke_color = nvgRGBA(150, 0, 0, 255);
            opts.fill_enabled = 1;
            opts.fill_color = nvgRGBA(255, 100, 100, 255);
            opts.seed = 123;
            
            float cx = 150 + x_spacing * 2, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Red Fill", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "2 strokes", nullptr);
        }

        // Semi-transparent
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(100, 0, 150, 255);
            opts.fill_enabled = 1;
            opts.fill_color = nvgRGBA(200, 100, 255, 180);
            
            float cx = 150 + x_spacing * 3, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Transparent", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "alpha: 180", nullptr);
        }

        // Outline only (no fill)
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.5f;
            opts.bowing = 1.3f;
            opts.stroke_count = 3;
            opts.stroke_width = 1.5f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            opts.fill_enabled = 0;
            
            float cx = 150 + x_spacing * 4, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 60, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 90, "Outline Only", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 110, "3 strokes", nullptr);
        }

        // Row 3: Different sizes
        y_start += y_spacing;

        // Small
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 1.5f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 30, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 60, "Small", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 80, "radius: 30", nullptr);
        }

        // Medium
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150 + x_spacing, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 50, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 80, "Medium", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 100, "radius: 50", nullptr);
        }

        // Large
        {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = 1.0f;
            opts.stroke_count = 1;
            opts.stroke_width = 2.5f;
            opts.stroke_color = nvgRGBA(0, 0, 0, 255);
            
            float cx = 150 + x_spacing * 2, cy = y_start;
            nvgRoughCircle(vg, cx, cy, 70, opts);
            
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 200));
            nvgText(vg, cx, cy + 100, "Large", nullptr);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
            nvgText(vg, cx, cy + 120, "radius: 70", nullptr);
        }

        nvgEndFrame(vg);
        SDL_GL_SwapWindow(window);
    }

    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
