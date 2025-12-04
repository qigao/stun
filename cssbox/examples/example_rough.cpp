/*
 * NanoVG Rough - Demo Example
 * 
 * Demonstrates hand-drawn style rendering using the rough module
 */

#include <nanovg.h>
#include <nanovg_rough.h>
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <stdio.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

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

    SDL_Window* window = SDL_CreateWindow("NanoVG Rough Demo",
        1000, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
        printf("Could not init nanovg.\n");
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

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
        float pxRatio = (float)fbWidth / (float)winWidth;

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

        // Title
        nvgFontSize(vg, 32.0f);
        nvgFontFace(vg, "sans-bold");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgText(vg, winWidth / 2, 20, "NanoVG Rough - Hand-Drawn Style", NULL);

        // Demo 1: Rough Circles
        nvgFontSize(vg, 18.0f);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(vg, 50, 80, "Rough Circles (varying roughness)", NULL);

        for (int i = 0; i < 4; i++) {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = i * 1.5f;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(50, 100, 200, 255);
            opts.stroke_enabled = 1;
            opts.seed = 42 + i;
            
            nvgRoughCircle(vg, 100 + i * 120, 150, 40, opts);
        }

        // Demo 2: Rough Rectangles with Fill
        nvgText(vg, 50, 250, "Rough Rectangles (with hachure fill)", NULL);

        for (int i = 0; i < 3; i++) {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.5f;
            opts.bowing = 1.0f + i * 0.5f;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(200, 50, 50, 255);
            opts.fill_color = nvgRGBA(255, 200, 200, 180);
            opts.stroke_enabled = 1;
            opts.fill_enabled = 1;
            opts.seed = 100 + i;
            
            nvgRoughRect(vg, 80 + i * 150, 320, 100, 80, opts);
        }

        // Demo 3: Rough Lines
        nvgText(vg, 50, 450, "Rough Lines (with bowing)", NULL);

        for (int i = 0; i < 3; i++) {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.0f;
            opts.bowing = i * 1.5f;
            opts.stroke_width = 3.0f;
            opts.stroke_color = nvgRGBA(50, 150, 50, 255);
            opts.stroke_count = 1 + i;
            opts.seed = 200 + i;
            
            nvgRoughLine(vg, 100, 500 + i * 60, 400, 500 + i * 60, opts);
        }

        // Demo 4: Rough Ellipses
        nvgText(vg, 550, 80, "Rough Ellipses", NULL);

        for (int i = 0; i < 3; i++) {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.roughness = 1.5f;
            opts.stroke_width = 2.0f;
            opts.stroke_color = nvgRGBA(150, 50, 150, 255);
            opts.fill_color = nvgRGBA(220, 180, 220, 150);
            opts.fill_enabled = (i == 1);
            opts.stroke_enabled = 1;
            opts.seed = 300 + i;
            
            nvgRoughEllipse(vg, 650 + i * 100, 150, 40, 60, opts);
        }

        // Demo 5: Multiple Strokes
        nvgText(vg, 550, 250, "Multiple Strokes (sketch effect)", NULL);

        NVGRoughOptions opts = nvgRoughDefaultOptions();
        opts.roughness = 2.0f;
        opts.stroke_width = 1.5f;
        opts.stroke_color = nvgRGBA(100, 100, 100, 180);
        opts.stroke_count = 3;
        opts.seed = 400;
        
        nvgRoughRect(vg, 600, 320, 120, 100, opts);

        // Demo 6: Rough Path
        nvgText(vg, 550, 450, "Rough Path (polygon)", NULL);

        float points[] = {
            650, 500,
            750, 520,
            780, 580,
            720, 620,
            650, 600
        };
        
        opts = nvgRoughDefaultOptions();
        opts.roughness = 1.5f;
        opts.stroke_width = 2.5f;
        opts.stroke_color = nvgRGBA(200, 100, 50, 255);
        opts.seed = 500;
        
        nvgRoughPath(vg, points, 5, 1, opts);

        nvgEndFrame(vg);

        SDL_GL_SwapWindow(window);
    }

    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
