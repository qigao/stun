/*
 * NanoVG Rough - Demo Example
 *
 * Demonstrates hand-drawn style rendering using the rough module
 */

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <nanovg.h>
#include <nanovg_rough.h>
#include <stdio.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

int main() {
    if (!glfwInit()) {
        printf("GLFW init failed");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1000, 800, "NanoVG Rough Demo", nullptr, nullptr);
    if (!window) {
        printf("Window creation failed");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);

    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        printf("Could not init nanovg.");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int winWidth, winHeight;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        float pxRatio = (float)fbWidth / (float)winWidth;

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

        nvgFontSize(vg, 32.0f);
        nvgFontFace(vg, "sans-bold");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgText(vg, winWidth / 2, 20, "NanoVG Rough - Hand-Drawn Style", NULL);

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

        nvgEndFrame(vg);
        glfwSwapBuffers(window);
    }

    nvgDeleteGL3(vg);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
