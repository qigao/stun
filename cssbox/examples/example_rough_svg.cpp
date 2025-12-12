/*
 * NanoVG CSS - Rough SVG Demo
 * 
 * Demonstrates hand-drawn style rendering via CSS/SVG API
 * Uses stroke-rendering: rough property
 */

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cssbox.h>
#include <cssbox_internal.h>
#include <stdio.h>

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        printf("GLFW init failed");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    // MSAA handled by GLFW_SAMPLES;

    GLFWwindow* window = glfwCreateWindow(800, 600, "Demo", nullptr, nullptr);
    if (!window) {
        printf("Window creation failed");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);

    // Create NanoVG context
    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        printf("Failed to create NanoVG context\n");
        
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Create NanoVG CSS renderer
    cssboxRenderer* renderer = cssboxCreateRenderer(vg);
    if (!renderer) {
        printf("Failed to create renderer\n");
        nvgDeleteGL3(vg);
        
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Create 3 circles with single stroke rendering
    auto* circle1 = cssboxCreateElement(renderer, "c1", "circle");
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
    
    auto* circle2 = cssboxCreateElement(renderer, "c2", "circle");
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
    
    auto* circle3 = cssboxCreateElement(renderer, "c3", "circle");
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
    cssboxUpdate(renderer, 0.0f);
    cssboxComputeLayout(renderer);

    printf("=== DEMO READY ===\n");
    printf("Circle 1: roughness=2.0, bowing=2.0, stroke-count=1\n");
    printf("Circle 2: roughness=2.5, bowing=2.5, stroke-count=1 (filled)\n");
    printf("Circle 3: roughness=3.0, bowing=3.5, stroke-count=1\n");

    // Main loop
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int winWidth, winHeight;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        cssboxSetViewport(renderer, (float)winWidth, (float)winHeight);

        if (!cssboxNeedsPaint(renderer)) {
            glfwWaitEventsTimeout(0.001);
            continue;
        }

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        float pixelRatio = (float)fbWidth / (float)winWidth;
        nvgBeginFrame(vg, winWidth, winHeight, pixelRatio);

        // Render (don't call cssboxUpdate - it would reset properties!)
        cssboxRender(renderer);

        nvgEndFrame(vg);
        glfwSwapBuffers(window);
    }

    // Cleanup
    cssboxDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
