/*
 * NanoVG CSS - Rough SVG Demo
 * 
 * Demonstrates hand-drawn style rendering via CSS/SVG API
 * Uses stroke-rendering: rough property
 */

#include <SDL3/SDL.h>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_css.h>
#include <nanovg_css_internal.h>
#include <stdio.h>

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* window = SDL_CreateWindow("NanoVG CSS - Rough SVG Demo",
        800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        printf("GL context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
    SDL_GL_SetSwapInterval(1);

    // Create NanoVG context
    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        printf("Failed to create NanoVG context\n");
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Create NanoVG CSS renderer
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(vg);
    if (!renderer) {
        printf("Failed to create renderer\n");
        nvgDeleteGL3(vg);
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Create 3 circles with single stroke rendering
    auto* circle1 = nvgcssCreateElement(renderer, "c1", "circle");
    circle1->inline_style["cx"] = "150";
    circle1->inline_style["cy"] = "300";
    circle1->inline_style["r"] = "60";
    circle1->inline_style["fill"] = "none";
    circle1->inline_style["stroke"] = "#3366cc";
    circle1->inline_style["stroke-width"] = "3";
    circle1->inline_style["stroke-rendering"] = "rough";
    circle1->inline_style["roughness"] = "2.0";
    circle1->inline_style["bowing"] = "2.0";
    circle1->inline_style["stroke-count"] = "1";  // Single stroke
    circle1->inline_style["seed"] = "42";
    circle1->visible = true;
    
    auto* circle2 = nvgcssCreateElement(renderer, "c2", "circle");
    circle2->inline_style["cx"] = "400";
    circle2->inline_style["cy"] = "300";
    circle2->inline_style["r"] = "60";
    circle2->inline_style["fill"] = "#ff9999";
    circle2->inline_style["stroke"] = "#cc3333";
    circle2->inline_style["stroke-width"] = "3";
    circle2->inline_style["stroke-rendering"] = "rough";
    circle2->inline_style["roughness"] = "2.5";
    circle2->inline_style["bowing"] = "2.5";
    circle2->inline_style["stroke-count"] = "1";  // Single stroke
    circle2->inline_style["seed"] = "123";
    circle2->visible = true;
    
    auto* circle3 = nvgcssCreateElement(renderer, "c3", "circle");
    circle3->inline_style["cx"] = "650";
    circle3->inline_style["cy"] = "300";
    circle3->inline_style["r"] = "60";
    circle3->inline_style["fill"] = "none";
    circle3->inline_style["stroke"] = "#cc66cc";
    circle3->inline_style["stroke-width"] = "3";
    circle3->inline_style["stroke-rendering"] = "rough";
    circle3->inline_style["roughness"] = "3.0";
    circle3->inline_style["bowing"] = "3.5";
    circle3->inline_style["stroke-count"] = "1";  // Single stroke
    circle3->inline_style["seed"] = "999";
    circle3->visible = true;
    
    // Update styles and compute layout ONCE
    nvgcssUpdate(renderer, 0.0f);
    nvgcssComputeLayout(renderer);

    printf("=== DEMO READY ===\n");
    printf("Circle 1: roughness=2.0, bowing=2.0, stroke-count=1\n");
    printf("Circle 2: roughness=2.5, bowing=2.5, stroke-count=1 (filled)\n");
    printf("Circle 3: roughness=3.0, bowing=3.5, stroke-count=1\n");

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        int winWidth, winHeight;
        SDL_GetWindowSize(window, &winWidth, &winHeight);
        int fbWidth, fbHeight;
        SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        float pixelRatio = (float)fbWidth / (float)winWidth;
        nvgBeginFrame(vg, winWidth, winHeight, pixelRatio);
        nvgcssSetViewport(renderer, (float)winWidth, (float)winHeight);
        
        // Render (don't call nvgcssUpdate - it would reset properties!)
        nvgcssRender(renderer);
        
        nvgEndFrame(vg);
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    nvgcssDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
