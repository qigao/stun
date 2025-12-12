/*
 * NanoVG CSS Example - Relative Layout
 *
 * Demonstrates relative positioning capabilities:
 * - Relative positioning (offset from normal flow)
 * - Space preservation in document flow
 * - Z-index stacking with positioned elements
 */

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Include GLAD for OpenGL function loading
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

// Include NanoVG with GL3 backend
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <cssbox.h>
#include <fmtlog.h>

class RelativeLayoutDemo {
public:
    RelativeLayoutDemo() {
        init_glfw();
        init_nanovg();
        setup_scene();
    }

    ~RelativeLayoutDemo() {
        if (renderer) cssboxDeleteRenderer(renderer);
        if (vg) nvgDeleteGL3(vg);
        
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }

    void run() {
        
        

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (render()) {
                glfwSwapBuffers(window);
            } else {
                glfwWaitEventsTimeout(0.001);
            }
        }
    }

    static void cursor_pos_callback(GLFWwindow* win, double xpos, double ypos) {
        // Mouse position callback
    }

    static void window_refresh_callback(GLFWwindow* win) {
        auto* app = static_cast<RelativeLayoutDemo*>(glfwGetWindowUserPointer(win));
        if (app) {
            app->render();
            glfwSwapBuffers(win);
        }
    }

private:
    GLFWwindow* window = nullptr;
    
    NVGcontext* vg = nullptr;
    cssboxRenderer* renderer = nullptr;

    void init_glfw() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(1200, 800, "NanoVG CSS Demo", nullptr, nullptr);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, cursor_pos_callback);
        glfwSetWindowRefreshCallback(window, window_refresh_callback);
    }

    void init_nanovg() {
        gladLoadGL();
        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

        // Load fonts (REQUIRED for text rendering!)
        // Assuming resources are in the same relative path as other examples
        if (nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf") == -1) {
            loge("Failed to load font 'sans-serif'");
        }
        if (nvgCreateFont(vg, "sans-serif-Bold", "resources/Roboto-Bold.ttf") == -1) {
            loge("Failed to load font 'sans-serif-Bold'");
        }

        renderer = cssboxCreateRenderer(vg);
    }

    void setup_scene() {
        const char* css = R"(
            /* Main Container */
            #container {
                width: 700px;
                height: 500px;
                background: #f5f5f5;
                border: 2px solid #ccc;
                margin: 20px;
                padding: 20px;
                position: relative;
            }

            /* Common Box Style */
            .box {
                width: 100px;
                height: 100px;
                border-radius: 8px;
                display: flex;
                justify-content: center;
                align-items: center;
                font-size: 14px;
                color: white;
                font-family: sans-serif-Bold;
            }

            /* Section 1: Basic Relative Positioning */
            #section-basic {
                margin-bottom: 40px;
                border: 1px dashed #999;
                padding: 10px;
            }

            .static-box {
                background: #9E9E9E;
            }

            .relative-box {
                background: #2196F3;
                position: relative;
                top: 20px;
                left: 20px;
                z-index: 10;
            }

            /* Section 2: Overlapping & Z-Index */
            #section-stacking {
                position: relative;
                height: 200px;
                border: 1px dashed #999;
                padding: 10px;
            }

            #box-1 {
                background: #F44336; /* Red */
                position: relative;
                top: 0;
                left: 0;
                z-index: 1;
            }

            #box-2 {
                background: #4CAF50; /* Green */
                position: relative;
                top: -50px;
                left: 50px;
                z-index: 2;
            }

            #box-3 {
                background: #FFC107; /* Amber */
                position: relative;
                top: -100px;
                left: 100px;
                z-index: 0; /* Should be behind others if they have higher z-index */
            }
            
            .label {
                color: #333;
                font-size: 16px;
                font-family: sans-serif-Bold;
                margin-bottom: 10px;
            }
        )";

        cssboxParseCSS(renderer, css);

        auto* container = cssboxCreateElement(renderer, "container", "div");

        // === Section 1: Basic Relative Positioning ===
        auto* label1 = cssboxCreateElement(renderer, "", "div");
        cssboxAddClass(label1, "label");
        cssboxSetText(label1, "1. Relative Positioning (Blue box offset by 20px)");
        cssboxAppendChild(renderer, container, label1);

        auto* section1 = cssboxCreateElement(renderer, "section-basic", "div");
        cssboxAppendChild(renderer, container, section1);

        // Static Box 1
        auto* static1 = cssboxCreateElement(renderer, "", "div");
        cssboxAddClass(static1, "box");
        cssboxAddClass(static1, "static-box");
        cssboxSetText(static1, "Static");
        cssboxAppendChild(renderer, section1, static1);

        // Relative Box (Offset)
        auto* relative1 = cssboxCreateElement(renderer, "", "div");
        cssboxAddClass(relative1, "box");
        cssboxAddClass(relative1, "relative-box");
        cssboxSetText(relative1, "Relative");
        cssboxAppendChild(renderer, section1, relative1);

        // Static Box 2 (Shows space preservation)
        auto* static2 = cssboxCreateElement(renderer, "", "div");
        cssboxAddClass(static2, "box");
        cssboxAddClass(static2, "static-box");
        cssboxSetText(static2, "Static");
        cssboxAppendChild(renderer, section1, static2);


        // === Section 2: Stacking ===
        auto* label2 = cssboxCreateElement(renderer, "", "div");
        cssboxAddClass(label2, "label");
        cssboxSetText(label2, "2. Stacking Context (Green z:2, Red z:1, Amber z:0)");
        cssboxAppendChild(renderer, container, label2);

        auto* section2 = cssboxCreateElement(renderer, "section-stacking", "div");
        cssboxAppendChild(renderer, container, section2);

        auto* box1 = cssboxCreateElement(renderer, "box-1", "div");
        cssboxAddClass(box1, "box");
        cssboxSetText(box1, "z-index: 1");
        cssboxAppendChild(renderer, section2, box1);

        auto* box2 = cssboxCreateElement(renderer, "box-2", "div");
        cssboxAddClass(box2, "box");
        cssboxSetText(box2, "z-index: 2");
        cssboxAppendChild(renderer, section2, box2);

        auto* box3 = cssboxCreateElement(renderer, "box-3", "div");
        cssboxAddClass(box3, "box");
        cssboxSetText(box3, "z-index: 0");
        cssboxAppendChild(renderer, section2, box3);
    }

    bool render() {
        int win_w, win_h, fb_w, fb_h;
        glfwGetWindowSize(window, &win_w, &win_h);
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        cssboxSetViewport(renderer, (float)win_w, (float)win_h);

        if (!cssboxNeedsPaint(renderer)) {
            return false;
        }

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);

        cssboxRender(renderer);

        nvgEndFrame(vg);

        return true;
    }
};

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);
    RelativeLayoutDemo demo;
    demo.run();
    return 0;
}
