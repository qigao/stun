/*
 * NanoVG CSS - Quadtree Layout Example
 *
 * Demonstrates the quadtree layout engine with:
 * - Nested flex containers
 * - Grid layout with explicit placement
 * - Grid spanning
 */

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <cssbox.h>
#include <cssbox_quadtree.h>
#include "cssbox_internal.h"
#include <fmtlog.h>
#include <iostream>

using namespace cssbox;

// Helper to create grid dashboard
cssboxElement* create_grid_dashboard(cssboxRenderer* renderer) {
    cssboxElement* dashboard = cssboxCreateElement(renderer, "dashboard", "div");
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
    cssboxElement* header = cssboxCreateElement(renderer, "grid-header", "div");
    header->inline_style["grid-column"] = "1 / 4";
    header->inline_style["grid-row"] = "1";
    header->inline_style["background"] = "rgb(102, 126, 234)";
    header->style.border.radius[0] = header->style.border.radius[1] = 
    header->style.border.radius[2] = header->style.border.radius[3] = 8;
    header->style.padding[0] = header->style.padding[1] = 
    header->style.padding[2] = header->style.padding[3] = Length::px(20);
    cssboxSetText(header, "Grid Dashboard");
    cssboxAppendChild(renderer, dashboard, header);
    
    // Sidebar - spans 2 rows
    cssboxElement* sidebar = cssboxCreateElement(renderer, "grid-sidebar", "div");
    sidebar->inline_style["grid-column"] = "1";
    sidebar->inline_style["grid-row"] = "2 / span 2";
    sidebar->inline_style["background"] = "rgb(255, 255, 255)";
    sidebar->style.border.radius[0] = sidebar->style.border.radius[1] = 
    sidebar->style.border.radius[2] = sidebar->style.border.radius[3] = 8;
    sidebar->style.padding[0] = sidebar->style.padding[1] = 
    sidebar->style.padding[2] = sidebar->style.padding[3] = Length::px(20);
    cssboxSetText(sidebar, "Sidebar\n(spans 2 rows)");
    cssboxAppendChild(renderer, dashboard, sidebar);
    
    // Main content - spans 2 columns
    cssboxElement* main = cssboxCreateElement(renderer, "grid-main", "div");
    main->inline_style["grid-column"] = "2 / span 2";
    main->inline_style["grid-row"] = "2";
    main->inline_style["background"] = "rgb(255, 255, 255)";
    main->style.border.radius[0] = main->style.border.radius[1] = 
    main->style.border.radius[2] = main->style.border.radius[3] = 8;
    main->style.padding[0] = main->style.padding[1] = 
    main->style.padding[2] = main->style.padding[3] = Length::px(20);
    cssboxSetText(main, "Main Content\n(spans 2 columns)");
    cssboxAppendChild(renderer, dashboard, main);
    
    // Footer left
    cssboxElement* footer1 = cssboxCreateElement(renderer, "grid-footer1", "div");
    footer1->inline_style["grid-column"] = "2";
    footer1->inline_style["grid-row"] = "3";
    footer1->inline_style["background"] = "rgb(255, 255, 255)";
    footer1->style.border.radius[0] = footer1->style.border.radius[1] = 
    footer1->style.border.radius[2] = footer1->style.border.radius[3] = 8;
    footer1->style.padding[0] = footer1->style.padding[1] = 
    footer1->style.padding[2] = footer1->style.padding[3] = Length::px(20);
    cssboxSetText(footer1, "Footer Left");
    cssboxAppendChild(renderer, dashboard, footer1);
    
    // Footer right
    cssboxElement* footer2 = cssboxCreateElement(renderer, "grid-footer2", "div");
    footer2->inline_style["grid-column"] = "3";
    footer2->inline_style["grid-row"] = "3";
    footer2->inline_style["background"] = "rgb(255, 255, 255)";
    footer2->style.border.radius[0] = footer2->style.border.radius[1] = 
    footer2->style.border.radius[2] = footer2->style.border.radius[3] = 8;
    footer2->style.padding[0] = footer2->style.padding[1] = 
    footer2->style.padding[2] = footer2->style.padding[3] = Length::px(20);
    cssboxSetText(footer2, "Footer Right");
    cssboxAppendChild(renderer, dashboard, footer2);
    
    return dashboard;
}

// Helper to create flex layout
cssboxElement* create_flex_layout(cssboxRenderer* renderer) {
    cssboxElement* root = cssboxCreateElement(renderer, "flex-root", "div");
    root->inline_style["display"] = "flex";
    root->inline_style["flex-direction"] = "column";
    root->inline_style["width"] = "100%";
    root->inline_style["height"] = "100%";
    root->inline_style["background"] = "rgb(240, 240, 240)";
    
    // Header
    cssboxElement* header = cssboxCreateElement(renderer, "flex-header", "div");
    header->inline_style["display"] = "flex";
    header->inline_style["flex-direction"] = "row";
    header->inline_style["height"] = "80px";
    header->inline_style["background"] = "rgb(118, 75, 162)";
    header->inline_style["padding"] = "20px";
    header->inline_style["gap"] = "20px";
    cssboxAppendChild(renderer, root, header);
    
    cssboxElement* title = cssboxCreateElement(renderer, "flex-title", "div");
    title->inline_style["flex-grow"] = "1";
    cssboxSetText(title, "Flex Layout");
    cssboxAppendChild(renderer, header, title);
    
    // Main
    cssboxElement* main = cssboxCreateElement(renderer, "flex-main", "div");
    main->inline_style["display"] = "flex";
    main->inline_style["flex-direction"] = "row";
    main->inline_style["flex-grow"] = "1";
    main->inline_style["gap"] = "20px";
    main->inline_style["padding"] = "20px";
    cssboxAppendChild(renderer, root, main);
    
    // Sidebar
    cssboxElement* sidebar = cssboxCreateElement(renderer, "flex-sidebar", "div");
    sidebar->inline_style["flex-basis"] = "200px";
    sidebar->inline_style["background"] = "rgb(255, 255, 255)";
    sidebar->style.border.radius[0] = sidebar->style.border.radius[1] = 
    sidebar->style.border.radius[2] = sidebar->style.border.radius[3] = 8;
    sidebar->inline_style["padding"] = "20px";
    cssboxSetText(sidebar, "Sidebar\n(flex-basis: 200px)");
    cssboxAppendChild(renderer, main, sidebar);
    
    // Content
    cssboxElement* content = cssboxCreateElement(renderer, "flex-content", "div");
    content->inline_style["flex-grow"] = "1";
    content->inline_style["background"] = "rgb(255, 255, 255)";
    content->style.border.radius[0] = content->style.border.radius[1] = 
    content->style.border.radius[2] = content->style.border.radius[3] = 8;
    content->inline_style["padding"] = "20px";
    cssboxSetText(content, "Content\n(flex-grow: 1)");
    cssboxAppendChild(renderer, main, content);
    
    return root;
}

int main() {
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    GLFWwindow* window = glfwCreateWindow(1400, 900, "Demo", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    
    glfwSwapInterval(1);

    gladLoadGL();

    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        std::cerr << "Failed to create NanoVG context" << std::endl;
        return -1;
    }

    nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf");

    cssboxRenderer* renderer = cssboxCreateRenderer(vg);
    cssboxSetViewport(renderer, 1400, 900);

    // Create both layouts (only one will be active at a time)
    cssboxElement* gridLayout = nullptr;
    cssboxElement* flexLayout = nullptr;
    cssboxElement* currentLayout = nullptr;
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Handle keyboard input for layout toggle
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            static bool space_was_pressed = false;
            if (!space_was_pressed) {
                space_was_pressed = true;
                showGrid = !showGrid;
                if (showGrid) {
                    currentLayout = gridLayout;
                } else {
                    if (!flexLayout) {
                        flexLayout = create_flex_layout(renderer);
                    }
                    currentLayout = flexLayout;
                }
                std::cout << "Switched to " << (showGrid ? "Grid" : "Flex") << " layout" << std::endl;
            }
        } else {
            static bool space_was_pressed = false;
            space_was_pressed = false;
        }

        int win_w, win_h, fb_w, fb_h;
        glfwGetWindowSize(window, &win_w, &win_h);
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        cssboxSetViewport(renderer, (float)win_w, (float)win_h);

        // Update styles (computes element->style from CSS/inline styles)
        cssboxUpdate(renderer, 0.016f);

        if (!cssboxNeedsPaint(renderer)) {
            glfwWaitEventsTimeout(0.001);
            continue;
        }

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);

        // Render using standard API (now uses quadtree internally)
        cssboxRender(renderer);

        nvgEndFrame(vg);
        glfwSwapBuffers(window);
    }

    delete qtEngine;
    cssboxDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
