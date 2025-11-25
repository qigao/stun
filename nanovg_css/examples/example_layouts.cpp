/*
 * NanoVG CSS Example - Layouts
 *
 * Demonstrates CSS layout capabilities:
 * - Flexbox (row, column, wrap, justify-content, align-items)
 * - Grid (template-rows/columns, areas, gap)
 * - Positioning (relative, absolute, fixed)
 *
 * Validates the typed CSS property system and 60fps refactor.
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

// Simple demo class
class LayoutsDemo {
public:
    LayoutsDemo() {
        init_sdl();
        init_nanovg();
        setup_scene();
    }

    ~LayoutsDemo() {
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

        window = SDL_CreateWindow("NanoVG CSS Layouts", 1200, 800,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);
    }

    void init_nanovg() {
        gladLoadGL();
        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

        // Load fonts (REQUIRED for text rendering!)
        if (nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf") == -1) {
            loge("Failed to load font 'sans-serif'");
        }
        if (nvgCreateFont(vg, "sans-serif-Bold", "resources/Roboto-Bold.ttf") == -1) {
            loge("Failed to load font 'sans-serif-Bold'");
        }

        renderer = nvgcssCreateRenderer(vg);
    }

    void setup_scene() {
        // All CSS styles inline - zero external dependencies
        const char* css = R"(
            /* === Section 1: Flexbox Row === */
            #flex-row {
                display: flex;
                flex-direction: row;
                gap: 10px;
                padding: 10px;
                width: 550px;
                height: 100px;
                background: #f0f0f0;
                border: 2px solid #333;
                border-radius: 8px;
                position: absolute;
                top: 20px;
                left: 20px;
            }
            .flex-item {
                width: 80px;
                height: 80px;
                background: #4CAF50;
                border-radius: 4px;
            }

            /* === Section 2: Flexbox Column === */
            #flex-column {
                display: flex;
                flex-direction: column;
                gap: 10px;
                padding: 10px;
                width: 120px;
                height: 340px;
                background: #f0f0f0;
                border: 2px solid #333;
                border-radius: 8px;
                position: absolute;
                top: 20px;
                left: 590px;
            }

            /* === Section 3: Grid Layout === */
            #grid-container {
                display: grid;
                grid-template-columns: 1fr 1fr 1fr;
                grid-template-rows: 80px 80px;
                gap: 10px;
                padding: 10px;
                width: 430px;
                height: 200px;
                background: #f0f0f0;
                border: 2px solid #333;
                border-radius: 8px;
                position: absolute;
                top: 140px;
                left: 20px;
            }
            .grid-item {
                background: #2196F3;
                border-radius: 4px;
            }

            /* === Section 4: Grid Areas === */
            #grid-areas {
                display: grid;
                grid-template-areas:
                    "header header header"
                    "sidebar main main"
                    "footer footer footer";
                grid-template-rows: 60px 120px 60px;
                grid-template-columns: 100px 1fr 1fr;
                gap: 10px;
                padding: 10px;
                width: 430px;
                height: 280px;
                background: #f0f0f0;
                border: 2px solid #333;
                border-radius: 8px;
                position: absolute;
                top: 360px;
                left: 20px;
            }
            #header { grid-area: header; background: #FF5722; border-radius: 4px; }
            #sidebar { grid-area: sidebar; background: #9C27B0; border-radius: 4px; }
            #main { grid-area: main; background: #00BCD4; border-radius: 4px; }
            #footer { grid-area: footer; background: #FFC107; border-radius: 4px; }

            /* === Section 5: Positioning Demo === */
            #position-container {
                position: absolute;
                width: 360px;
                height: 300px;
                background: #f0f0f0;
                border: 2px solid #333;
                border-radius: 8px;
                top: 140px;
                left: 470px;
            }
            #pos-relative {
                position: relative;
                top: 20px;
                left: 20px;
                width: 100px;
                height: 60px;
                background: #E91E63;
                border-radius: 4px;
            }
            #pos-absolute {
                position: absolute;
                top: 100px;
                right: 20px;
                width: 100px;
                height: 60px;
                background: #673AB7;
                border-radius: 4px;
            }

            /* === Labels === */
            .label {
                position: absolute;
                color: #333;
                font-size: 14px;
                font-weight: bold;
            }
            #label-flexrow { top: 5px; left: 25px; }
            #label-flexcol { top: 5px; left: 595px; }
            #label-grid { top: 125px; left: 25px; }
            #label-areas { top: 345px; left: 25px; }
            #label-pos { top: 125px; left: 475px; }
        )";

        nvgcssParseCSS(renderer, css);

        // === Create Flexbox Row ===
        auto* flex_row = nvgcssCreateElement(renderer, "flex-row", "div");
        for (int i = 0; i < 5; i++) {
            auto* item = nvgcssCreateElement(renderer, "", "div");
            nvgcssAddClass(item, "flex-item");
            nvgcssAppendChild(renderer, flex_row, item);
        }

        // === Create Flexbox Column ===
        auto* flex_col = nvgcssCreateElement(renderer, "flex-column", "div");
        for (int i = 0; i < 3; i++) {
            auto* item = nvgcssCreateElement(renderer, "", "div");
            nvgcssAddClass(item, "flex-item");
            nvgcssAppendChild(renderer, flex_col, item);
        }

        // === Create Grid Layout ===
        auto* grid = nvgcssCreateElement(renderer, "grid-container", "div");
        for (int i = 0; i < 6; i++) {
            auto* item = nvgcssCreateElement(renderer, "", "div");
            nvgcssAddClass(item, "grid-item");
            nvgcssAppendChild(renderer, grid, item);
        }

        // === Create Grid Areas ===
        auto* grid_areas = nvgcssCreateElement(renderer, "grid-areas", "div");
        nvgcssAppendChild(renderer, grid_areas, nvgcssCreateElement(renderer, "header", "div"));
        nvgcssAppendChild(renderer, grid_areas, nvgcssCreateElement(renderer, "sidebar", "div"));
        nvgcssAppendChild(renderer, grid_areas, nvgcssCreateElement(renderer, "main", "div"));
        nvgcssAppendChild(renderer, grid_areas, nvgcssCreateElement(renderer, "footer", "div"));

        // === Create Positioning Demo ===
        auto* pos_container = nvgcssCreateElement(renderer, "position-container", "div");
        nvgcssAppendChild(renderer, pos_container, nvgcssCreateElement(renderer, "pos-relative", "div"));
        nvgcssAppendChild(renderer, pos_container, nvgcssCreateElement(renderer, "pos-absolute", "div"));

        // === Create Labels ===
        auto create_label = [&](const char* id, const char* text) {
            auto* label = nvgcssCreateElement(renderer, id, "div");
            nvgcssAddClass(label, "label");
            nvgcssSetText(label, text);
            return label;
        };

        create_label("label-flexrow", "Flexbox Row");
        create_label("label-flexcol", "Flexbox Column");
        create_label("label-grid", "Grid Layout");
        create_label("label-areas", "Grid Areas");
        create_label("label-pos", "Positioning");
    }

    void render() {
        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        nvgcssSetViewport(renderer, (float)win_w, (float)win_h);
        nvgcssRender(renderer);
        nvgEndFrame(vg);
    }
};

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);  // Use DBG for detailed rendering traces
    LayoutsDemo demo;
    demo.run();
    return 0;
}
