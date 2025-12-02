/*
 * NanoVG CSS - Quadtree Layout Example
 *
 * Demonstrates the quadtree layout engine with:
 * - Nested flex containers
 * - Grid layout with explicit placement
 * - Grid spanning
 */

#include <SDL3/SDL.h>

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <nanovg_css.h>
#include <nanovg_css_quadtree.h>
#include "nanovg_css_internal.h"
#include <fmtlog.h>
#include <iostream>

using namespace nvgcss;

// Helper to create grid dashboard
NVGCSSElement* create_grid_dashboard(NVGCSSRenderer* renderer) {
    NVGCSSElement* dashboard = nvgcssCreateElement(renderer, "dashboard", "div");
    dashboard->inline_style["display"] = "grid";
    dashboard->inline_style["grid-template-columns"] = "200px 1fr 1fr";
    dashboard->inline_style["grid-template-rows"] = "80px 1fr 1fr";
    dashboard->inline_style["grid-column-gap"] = "10px";
    dashboard->inline_style["grid-row-gap"] = "10px";
    dashboard->inline_style["width"] = "100%";
    dashboard->inline_style["height"] = "100%";
    dashboard->inline_style["padding"] = "10px";
    dashboard->inline_style["background"] = "rgb(240, 240, 240)";
    
    // Header - spans all columns
    NVGCSSElement* header = nvgcssCreateElement(renderer, "grid-header", "div");
    header->inline_style["grid-column"] = "1 / 4";
    header->inline_style["grid-row"] = "1";
    header->inline_style["background"] = "rgb(102, 126, 234)";
    header->style.border.radius[0] = header->style.border.radius[1] = 
    header->style.border.radius[2] = header->style.border.radius[3] = 8;
    header->style.padding[0] = header->style.padding[1] = 
    header->style.padding[2] = header->style.padding[3] = Length::px(20);
    nvgcssSetText(header, "Grid Dashboard");
    nvgcssAppendChild(renderer, dashboard, header);
    
    // Sidebar - spans 2 rows
    NVGCSSElement* sidebar = nvgcssCreateElement(renderer, "grid-sidebar", "div");
    sidebar->inline_style["grid-column"] = "1";
    sidebar->inline_style["grid-row"] = "2 / span 2";
    sidebar->inline_style["background"] = "rgb(255, 255, 255)";
    sidebar->style.border.radius[0] = sidebar->style.border.radius[1] = 
    sidebar->style.border.radius[2] = sidebar->style.border.radius[3] = 8;
    sidebar->style.padding[0] = sidebar->style.padding[1] = 
    sidebar->style.padding[2] = sidebar->style.padding[3] = Length::px(20);
    nvgcssSetText(sidebar, "Sidebar\n(spans 2 rows)");
    nvgcssAppendChild(renderer, dashboard, sidebar);
    
    // Main content - spans 2 columns
    NVGCSSElement* main = nvgcssCreateElement(renderer, "grid-main", "div");
    main->inline_style["grid-column"] = "2 / span 2";
    main->inline_style["grid-row"] = "2";
    main->inline_style["background"] = "rgb(255, 255, 255)";
    main->style.border.radius[0] = main->style.border.radius[1] = 
    main->style.border.radius[2] = main->style.border.radius[3] = 8;
    main->style.padding[0] = main->style.padding[1] = 
    main->style.padding[2] = main->style.padding[3] = Length::px(20);
    nvgcssSetText(main, "Main Content\n(spans 2 columns)");
    nvgcssAppendChild(renderer, dashboard, main);
    
    // Footer left
    NVGCSSElement* footer1 = nvgcssCreateElement(renderer, "grid-footer1", "div");
    footer1->inline_style["grid-column"] = "2";
    footer1->inline_style["grid-row"] = "3";
    footer1->inline_style["background"] = "rgb(255, 255, 255)";
    footer1->style.border.radius[0] = footer1->style.border.radius[1] = 
    footer1->style.border.radius[2] = footer1->style.border.radius[3] = 8;
    footer1->style.padding[0] = footer1->style.padding[1] = 
    footer1->style.padding[2] = footer1->style.padding[3] = Length::px(20);
    nvgcssSetText(footer1, "Footer Left");
    nvgcssAppendChild(renderer, dashboard, footer1);
    
    // Footer right
    NVGCSSElement* footer2 = nvgcssCreateElement(renderer, "grid-footer2", "div");
    footer2->inline_style["grid-column"] = "3";
    footer2->inline_style["grid-row"] = "3";
    footer2->inline_style["background"] = "rgb(255, 255, 255)";
    footer2->style.border.radius[0] = footer2->style.border.radius[1] = 
    footer2->style.border.radius[2] = footer2->style.border.radius[3] = 8;
    footer2->style.padding[0] = footer2->style.padding[1] = 
    footer2->style.padding[2] = footer2->style.padding[3] = Length::px(20);
    nvgcssSetText(footer2, "Footer Right");
    nvgcssAppendChild(renderer, dashboard, footer2);
    
    return dashboard;
}

// Helper to create flex layout
NVGCSSElement* create_flex_layout(NVGCSSRenderer* renderer) {
    NVGCSSElement* root = nvgcssCreateElement(renderer, "flex-root", "div");
    root->inline_style["display"] = "flex";
    root->inline_style["flex-direction"] = "column";
    root->inline_style["width"] = "100%";
    root->inline_style["height"] = "100%";
    root->inline_style["background"] = "rgb(240, 240, 240)";
    
    // Header
    NVGCSSElement* header = nvgcssCreateElement(renderer, "flex-header", "div");
    header->inline_style["display"] = "flex";
    header->inline_style["flex-direction"] = "row";
    header->inline_style["height"] = "80px";
    header->inline_style["background"] = "rgb(118, 75, 162)";
    header->inline_style["padding"] = "20px";
    header->inline_style["gap"] = "20px";
    nvgcssAppendChild(renderer, root, header);
    
    NVGCSSElement* title = nvgcssCreateElement(renderer, "flex-title", "div");
    title->inline_style["flex-grow"] = "1";
    nvgcssSetText(title, "Flex Layout");
    nvgcssAppendChild(renderer, header, title);
    
    // Main
    NVGCSSElement* main = nvgcssCreateElement(renderer, "flex-main", "div");
    main->inline_style["display"] = "flex";
    main->inline_style["flex-direction"] = "row";
    main->inline_style["flex-grow"] = "1";
    main->inline_style["gap"] = "20px";
    main->inline_style["padding"] = "20px";
    nvgcssAppendChild(renderer, root, main);
    
    // Sidebar
    NVGCSSElement* sidebar = nvgcssCreateElement(renderer, "flex-sidebar", "div");
    sidebar->inline_style["flex-basis"] = "200px";
    sidebar->inline_style["background"] = "rgb(255, 255, 255)";
    sidebar->style.border.radius[0] = sidebar->style.border.radius[1] = 
    sidebar->style.border.radius[2] = sidebar->style.border.radius[3] = 8;
    sidebar->inline_style["padding"] = "20px";
    nvgcssSetText(sidebar, "Sidebar\n(flex-basis: 200px)");
    nvgcssAppendChild(renderer, main, sidebar);
    
    // Content
    NVGCSSElement* content = nvgcssCreateElement(renderer, "flex-content", "div");
    content->inline_style["flex-grow"] = "1";
    content->inline_style["background"] = "rgb(255, 255, 255)";
    content->style.border.radius[0] = content->style.border.radius[1] = 
    content->style.border.radius[2] = content->style.border.radius[3] = 8;
    content->inline_style["padding"] = "20px";
    nvgcssSetText(content, "Content\n(flex-grow: 1)");
    nvgcssAppendChild(renderer, main, content);
    
    return root;
}

int main() {
    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("Quadtree Layout Demo", 1400, 900,
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

    nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf");

    NVGCSSRenderer* renderer = nvgcssCreateRenderer(vg);
    nvgcssSetViewport(renderer, 1400, 900);

    // Create both layouts (only one will be active at a time)
    NVGCSSElement* gridLayout = nullptr;
    NVGCSSElement* flexLayout = nullptr;
    NVGCSSElement* currentLayout = nullptr;
    bool showGrid = true;
    
    // Create initial layout
    gridLayout = create_grid_dashboard(renderer);
    currentLayout = gridLayout;

    QuadtreeLayoutEngine* qtEngine = new QuadtreeLayoutEngine(1400, 900);

    std::cout << "Quadtree Layout Demo" << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << "Press SPACE to toggle between Grid and Flex layouts" << std::endl;
    std::cout << "Grid: Explicit placement + spanning" << std::endl;
    std::cout << "Flex: Nested containers with flex-grow" << std::endl;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_SPACE) {
                    showGrid = !showGrid;
                    if (showGrid) {
                        if (!gridLayout) {
                            gridLayout = create_grid_dashboard(renderer);
                        }
                        currentLayout = gridLayout;
                    } else {
                        if (!flexLayout) {
                            flexLayout = create_flex_layout(renderer);
                        }
                        currentLayout = flexLayout;
                    }
                    std::cout << "Switched to " << (showGrid ? "Grid" : "Flex") << " layout" << std::endl;
                }
            }
        }

        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);

        nvgcssSetViewport(renderer, (float)win_w, (float)win_h);

        // Update styles (computes element->style from CSS/inline styles)
        nvgcssUpdate(renderer, 0.016f);
        
        // Render using standard API (now uses quadtree internally)
        nvgcssRender(renderer);

        nvgEndFrame(vg);
        SDL_GL_SwapWindow(window);
    }

    delete qtEngine;
    nvgcssDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
