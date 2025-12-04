/*
 * SVG Features Demo
 * 
 * Demonstrates:
 * - SVG Patterns (Phase 8)
 * - Text on Path (Phase 6)
 * - SVG Markers (Phase 5)
 * - Clipping Paths (Phase 7)
 */

#include <SDL3/SDL.h>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cssbox.h>
#include <fmtlog.h>
#include <iostream>
#include <cssbox_internal.h>

int main() {
    // Disable verbose logs for clean output
    fmtlog::setLogLevel(fmtlog::WRN);
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("SVG Features - Patterns, Text-on-Path, Markers, Clipping", 1200, 800, SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "Failed to create window\n";
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    gladLoadGL();
    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    // Load font for text-on-path rendering
    if (nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf") == -1) {
        std::cerr << "Warning: Could not load font, text-on-path will not render\n";
    }

    auto* renderer = cssboxCreateRenderer(vg);
    cssboxSetViewport(renderer, 1200, 800);

    // ========================================================================
    // Demo 1: SVG Patterns
    // ========================================================================
    
    // Create a dots pattern
    auto* dots_pattern = cssboxCreatePattern(renderer, "dots", 0, 0, 20, 20);
    auto* dot = cssboxCreateElement(renderer, "dot1", "circle");
    dot->inline_style["cx"] = "10px";
    dot->inline_style["cy"] = "10px";
    dot->inline_style["r"] = "4px";
    dot->inline_style["fill"] = "#4a90e2";
    dot->inline_style["width"] = "20px";   // SVG elements need layout dimensions
    dot->inline_style["height"] = "20px";
    cssboxAppendChild(renderer, dots_pattern, dot);

    // Create a stripes pattern
    auto* stripes_pattern = cssboxCreatePattern(renderer, "stripes", 0, 0, 10, 10);
    auto* stripe = cssboxCreateElement(renderer, "stripe1", "rect");
    stripe->inline_style["left"] = "0";
    stripe->inline_style["top"] = "0";
    stripe->inline_style["width"] = "5px";
    stripe->inline_style["height"] = "10px";
    stripe->inline_style["fill"] = "#e74c3c";
    cssboxAppendChild(renderer, stripes_pattern, stripe);

    // Rectangle with dots pattern
    auto* pattern_rect1 = cssboxCreateElement(renderer, "pr1", "rect");
    pattern_rect1->inline_style["left"] = "50px";
    pattern_rect1->inline_style["top"] = "50px";
    pattern_rect1->inline_style["width"] = "200px";
    pattern_rect1->inline_style["height"] = "150px";
    pattern_rect1->inline_style["fill"] = "url(#dots)";
    pattern_rect1->inline_style["stroke"] = "#2c3e50";
    pattern_rect1->inline_style["stroke-width"] = "2px";

    // Circle with stripes pattern
    auto* pattern_circle = cssboxCreateElement(renderer, "pc1", "circle");
    pattern_circle->inline_style["cx"] = "400px";
    pattern_circle->inline_style["cy"] = "125px";
    pattern_circle->inline_style["r"] = "75px";
    pattern_circle->inline_style["fill"] = "url(#stripes)";
    pattern_circle->inline_style["stroke"] = "#2c3e50";
    pattern_circle->inline_style["stroke-width"] = "2px";

    // ========================================================================
    // Demo 2: Text on Path
    // ========================================================================
    
    // Create a curved path
    auto* curve_path = cssboxCreateElement(renderer, "curve1", "path");
    curve_path->inline_style["d"] = "M 50 300 Q 300 200 550 300";
    curve_path->inline_style["stroke"] = "#95a5a6";
    curve_path->inline_style["stroke-width"] = "2px";
    curve_path->inline_style["fill"] = "none";
    curve_path->inline_style["stroke-dasharray"] = "5 3";

    // Text following the path
    auto* text_path1 = cssboxCreateElement(renderer, "tp1", "textPath");
    text_path1->attributes["href"] = "#curve1";
    text_path1->text_content = "Text following a curved path!";
    text_path1->inline_style["fill"] = "#2c3e50";
    text_path1->inline_style["font-size"] = "24px";

    // Another path with offset
    auto* wave_path = cssboxCreateElement(renderer, "wave1", "path");
    wave_path->inline_style["d"] = "M 50 450 Q 150 400 250 450 T 450 450";
    wave_path->inline_style["stroke"] = "#95a5a6";
    wave_path->inline_style["stroke-width"] = "2px";
    wave_path->inline_style["fill"] = "none";

    auto* text_path2 = cssboxCreateElement(renderer, "tp2", "textPath");
    text_path2->attributes["href"] = "#wave1";
    text_path2->attributes["startOffset"] = "10%";
    text_path2->text_content = "Wave Pattern Text";
    text_path2->inline_style["fill"] = "#e74c3c";
    text_path2->inline_style["font-size"] = "20px";
    text_path2->inline_style["text-anchor"] = "middle";

    // ========================================================================
    // Demo 3: SVG Markers
    // ========================================================================
    
    // Create arrow marker
    auto* arrow_marker = cssboxCreateMarker(renderer, "arrow", 10, 10, 5, 5, "auto");
    auto* arrow_path = cssboxCreateElement(renderer, "arrow_shape", "path");
    arrow_path->inline_style["d"] = "M 0 0 L 10 5 L 0 10 Z";
    arrow_path->inline_style["fill"] = "#27ae60";
    cssboxAppendChild(renderer, arrow_marker, arrow_path);

    // Create dot marker
    auto* dot_marker = cssboxCreateMarker(renderer, "dot", 8, 8, 4, 4, "0");
    auto* marker_circle = cssboxCreateElement(renderer, "marker_dot", "circle");
    marker_circle->inline_style["cx"] = "4px";
    marker_circle->inline_style["cy"] = "4px";
    marker_circle->inline_style["r"] = "3px";
    marker_circle->inline_style["fill"] = "#e74c3c";
    marker_circle->inline_style["width"] = "8px";   // SVG elements need layout dimensions
    marker_circle->inline_style["height"] = "8px";
    cssboxAppendChild(renderer, dot_marker, marker_circle);

    // Path with markers
    auto* marker_path = cssboxCreateElement(renderer, "mp1", "path");
    marker_path->inline_style["d"] = "M 650 100 L 750 100 L 800 150 L 850 100";
    marker_path->inline_style["stroke"] = "#27ae60";
    marker_path->inline_style["stroke-width"] = "3px";
    marker_path->inline_style["fill"] = "none";
    marker_path->inline_style["marker-start"] = "url(#dot)";
    marker_path->inline_style["marker-mid"] = "url(#dot)";
    marker_path->inline_style["marker-end"] = "url(#arrow)";

    // ========================================================================
    // Demo 4: Clipping Paths
    // ========================================================================
    
    // Create clip path
    auto* clip = cssboxCreateClipPath(renderer, "clip1");
    auto* clip_rect = cssboxCreateElement(renderer, "clip_shape", "rect");
    clip_rect->inline_style["left"] = "650px";
    clip_rect->inline_style["top"] = "250px";
    clip_rect->inline_style["width"] = "200px";
    clip_rect->inline_style["height"] = "150px";
    cssboxAppendChild(renderer, clip, clip_rect);

    // Element with clipping
    auto* clipped_circle = cssboxCreateElement(renderer, "cc1", "circle");
    clipped_circle->inline_style["cx"] = "750px";
    clipped_circle->inline_style["cy"] = "325px";
    clipped_circle->inline_style["r"] = "100px";
    clipped_circle->inline_style["fill"] = "#9b59b6";
    clipped_circle->inline_style["clip-path"] = "url(#clip1)";

    // ========================================================================
    // Labels
    // ========================================================================
    
    auto* label1 = cssboxCreateElement(renderer, "l1", "div");
    label1->text_content = "SVG Patterns";
    label1->inline_style["left"] = "50px";
    label1->inline_style["top"] = "20px";
    label1->inline_style["font-size"] = "18px";
    label1->inline_style["color"] = "#2c3e50";

    auto* label2 = cssboxCreateElement(renderer, "l2", "div");
    label2->text_content = "Text on Path";
    label2->inline_style["left"] = "50px";
    label2->inline_style["top"] = "270px";
    label2->inline_style["font-size"] = "18px";
    label2->inline_style["color"] = "#2c3e50";

    auto* label3 = cssboxCreateElement(renderer, "l3", "div");
    label3->text_content = "SVG Markers";
    label3->inline_style["left"] = "650px";
    label3->inline_style["top"] = "70px";
    label3->inline_style["font-size"] = "18px";
    label3->inline_style["color"] = "#2c3e50";

    auto* label4 = cssboxCreateElement(renderer, "l4", "div");
    label4->text_content = "Clipping Paths";
    label4->inline_style["left"] = "650px";
    label4->inline_style["top"] = "220px";
    label4->inline_style["font-size"] = "18px";
    label4->inline_style["color"] = "#2c3e50";

    std::cout << "SVG Features Demo\n";
    std::cout << "- Patterns: Dots and stripes\n";
    std::cout << "- Text on Path: Curved text\n";
    std::cout << "- Markers: Arrows and dots on paths\n";
    std::cout << "- Clipping: Circle clipped to rectangle\n";
    std::cout << "Press ESC or close window to exit\n";

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                       event.type == SDL_EVENT_WINDOW_RESTORED ||
                       event.type == SDL_EVENT_WINDOW_SHOWN) {
                cssboxInvalidatePaint(renderer);
            }
        }

        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        cssboxSetViewport(renderer, (float)win_w, (float)win_h);

        if (!cssboxNeedsPaint(renderer)) {
            SDL_Delay(1);
            continue;
        }

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Pre-render patterns before main NVG frame (required for FBO-based patterns)
        cssboxPreparePatterns(renderer);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxUpdate(renderer, 0.016f);  // 60fps delta time
        cssboxRender(renderer);
        nvgEndFrame(vg);

        SDL_GL_SwapWindow(window);
    }

    cssboxDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
