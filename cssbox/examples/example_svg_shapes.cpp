/*
 * Example: SVG Shapes with Stroke Styles
 * 
 * Demonstrates:
 * - Cubic bezier curves
 * - Stroke dash patterns
 * - Various line caps
 * - Filled and stroked shapes
 */

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cssbox.h>
#include <iostream>
#include <cssbox_internal.h>
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    
    glfwSwapInterval(1);

    gladLoadGL();
    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    
    auto* renderer = cssboxCreateRenderer(vg);
    cssboxSetViewport(renderer, 800, 600);

    // Cubic bezier curve
    auto* path1 = cssboxCreateElement(renderer, "curve1", "path");
    path1->inline_style["d"] = "M 100 300 C 100 100, 400 100, 400 300";
    path1->inline_style["stroke"] = "blue";
    path1->inline_style["stroke-width"] = "3px";
    path1->inline_style["fill"] = "none";

    // Dashed line
    auto* line1 = cssboxCreateElement(renderer, "line1", "line");
    line1->inline_style["x1"] = "100px";
    line1->inline_style["y1"] = "400px";
    line1->inline_style["x2"] = "500px";
    line1->inline_style["y2"] = "400px";
    line1->inline_style["stroke"] = "red";
    line1->inline_style["stroke-width"] = "3px";
    line1->inline_style["stroke-dasharray"] = "10 5";

    // Closed path with fill
    auto* path3 = cssboxCreateElement(renderer, "curve3", "path");
    path3->inline_style["d"] = "M 550 200 C 600 150, 650 150, 700 200 L 700 300 C 650 350, 600 350, 550 300 Z";
    path3->inline_style["stroke"] = "green";
    path3->inline_style["stroke-width"] = "2px";
    path3->inline_style["fill"] = "rgba(0, 255, 0, 0.2)";

    // Dotted line
    auto* line2 = cssboxCreateElement(renderer, "line2", "line");
    line2->inline_style["x1"] = "100px";
    line2->inline_style["y1"] = "450px";
    line2->inline_style["x2"] = "500px";
    line2->inline_style["y2"] = "450px";
    line2->inline_style["stroke"] = "purple";
    line2->inline_style["stroke-width"] = "3px";
    line2->inline_style["stroke-dasharray"] = "2 8";

    // Dashed circle
    auto* circle1 = cssboxCreateElement(renderer, "circle1", "circle");
    circle1->inline_style["cx"] = "150px";
    circle1->inline_style["cy"] = "150px";
    circle1->inline_style["r"] = "40px";
    circle1->inline_style["stroke"] = "orange";
    circle1->inline_style["stroke-width"] = "2px";
    circle1->inline_style["stroke-dasharray"] = "8 4";
    circle1->inline_style["fill"] = "none";

    // Dotted rectangle
    auto* rect1 = cssboxCreateElement(renderer, "rect1", "rect");
    rect1->inline_style["left"] = "550px";
    rect1->inline_style["top"] = "400px";
    rect1->inline_style["width"] = "100px";
    rect1->inline_style["height"] = "60px";
    rect1->inline_style["stroke"] = "brown";
    rect1->inline_style["stroke-width"] = "2px";
    rect1->inline_style["stroke-dasharray"] = "3 6";
    rect1->inline_style["fill"] = "none";

    cssboxUpdate(renderer, 0.0f);
    cssboxComputeLayout(renderer);

    std::cout << "SVG Shapes Example\n";
    std::cout << "- Blue: Cubic bezier curve (solid)\n";
    std::cout << "- Red: Dashed line\n";
    std::cout << "- Purple: Dotted line\n";
    std::cout << "- Green: Closed path with fill\n";
    std::cout << "- Orange: Dashed circle\n";
    std::cout << "- Brown: Dotted rectangle\n";
    std::cout << "Press ESC or close window to exit\n";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int win_w, win_h, fb_w, fb_h;
        glfwGetWindowSize(window, &win_w, &win_h);
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        cssboxSetViewport(renderer, (float)win_w, (float)win_h);

        if (!cssboxNeedsPaint(renderer)) {
            glfwWaitEventsTimeout(0.001);
            continue;
        }

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxRender(renderer);
        nvgEndFrame(vg);

        glfwSwapBuffers(window);
    }

    cssboxDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
