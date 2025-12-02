/*
 * NanoVG CSS Example - Relative Layout
 *
 * Demonstrates relative positioning capabilities:
 * - Relative positioning (offset from normal flow)
 * - Space preservation in document flow
 * - Z-index stacking with positioned elements
 */

#include <SDL3/SDL.h>

// Include GLAD for OpenGL function loading
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

// Include NanoVG with GL3 backend
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <nanovg_css.h>
#include <fmtlog.h>

class RelativeLayoutDemo {
public:
    RelativeLayoutDemo() {
        init_sdl();
        init_nanovg();
        setup_scene();
    }

    ~RelativeLayoutDemo() {
        if (renderer) nvgcssDeleteRenderer(renderer);
        if (vg) nvgDeleteGL3(vg);
        if (gl_context) SDL_GL_DestroyContext(gl_context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        bool running = true;
        SDL_Event event;

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) running = false;
            }

            render();
            SDL_GL_SwapWindow(window);
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    NVGCSSRenderer* renderer = nullptr;

    void init_sdl() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow("NanoVG CSS Relative Layout", 800, 600,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);
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

        renderer = nvgcssCreateRenderer(vg);
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

        nvgcssParseCSS(renderer, css);

        auto* container = nvgcssCreateElement(renderer, "container", "div");

        // === Section 1: Basic Relative Positioning ===
        auto* label1 = nvgcssCreateElement(renderer, "", "div");
        nvgcssAddClass(label1, "label");
        nvgcssSetText(label1, "1. Relative Positioning (Blue box offset by 20px)");
        nvgcssAppendChild(renderer, container, label1);

        auto* section1 = nvgcssCreateElement(renderer, "section-basic", "div");
        nvgcssAppendChild(renderer, container, section1);

        // Static Box 1
        auto* static1 = nvgcssCreateElement(renderer, "", "div");
        nvgcssAddClass(static1, "box");
        nvgcssAddClass(static1, "static-box");
        nvgcssSetText(static1, "Static");
        nvgcssAppendChild(renderer, section1, static1);

        // Relative Box (Offset)
        auto* relative1 = nvgcssCreateElement(renderer, "", "div");
        nvgcssAddClass(relative1, "box");
        nvgcssAddClass(relative1, "relative-box");
        nvgcssSetText(relative1, "Relative");
        nvgcssAppendChild(renderer, section1, relative1);

        // Static Box 2 (Shows space preservation)
        auto* static2 = nvgcssCreateElement(renderer, "", "div");
        nvgcssAddClass(static2, "box");
        nvgcssAddClass(static2, "static-box");
        nvgcssSetText(static2, "Static");
        nvgcssAppendChild(renderer, section1, static2);


        // === Section 2: Stacking ===
        auto* label2 = nvgcssCreateElement(renderer, "", "div");
        nvgcssAddClass(label2, "label");
        nvgcssSetText(label2, "2. Stacking Context (Green z:2, Red z:1, Amber z:0)");
        nvgcssAppendChild(renderer, container, label2);

        auto* section2 = nvgcssCreateElement(renderer, "section-stacking", "div");
        nvgcssAppendChild(renderer, container, section2);

        auto* box1 = nvgcssCreateElement(renderer, "box-1", "div");
        nvgcssAddClass(box1, "box");
        nvgcssSetText(box1, "z-index: 1");
        nvgcssAppendChild(renderer, section2, box1);

        auto* box2 = nvgcssCreateElement(renderer, "box-2", "div");
        nvgcssAddClass(box2, "box");
        nvgcssSetText(box2, "z-index: 2");
        nvgcssAppendChild(renderer, section2, box2);

        auto* box3 = nvgcssCreateElement(renderer, "box-3", "div");
        nvgcssAddClass(box3, "box");
        nvgcssSetText(box3, "z-index: 0");
        nvgcssAppendChild(renderer, section2, box3);
    }

    void render() {
        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        
        nvgcssSetViewport(renderer, (float)win_w, (float)win_h);
        nvgcssRender(renderer);

        nvgEndFrame(vg);
    }
};

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);
    RelativeLayoutDemo demo;
    demo.run();
    return 0;
}
