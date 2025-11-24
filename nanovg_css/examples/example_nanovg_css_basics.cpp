/*
 * FlexUI Example - NanoVG CSS Basics
 *
 * Demonstrates core nanovg_css features:
 * - CSS parsing and selectors
 * - Box model (padding, margin, border)
 * - Colors and gradients
 * - Pseudo-states (:hover, :active)
 * - CSS variables
 * - Transforms
 */

#include <SDL3/SDL.h>

// Include GLAD for OpenGL function loading and headers
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

// Include NanoVG with GL3 backend
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <nanovg_css.h>
#include "nanovg_css_internal.h"  // For computed layout access
#include <fmtlog.h>
#include <fstream>
#include <sstream>
#include <chrono>



class NanoVGCSSBasicsDemo {
public:
    NanoVGCSSBasicsDemo() {
        init_sdl();
        init_nanovg();
        setup_css();
        create_elements();
    }

    ~NanoVGCSSBasicsDemo() {
        if (renderer) nvgcssDeleteRenderer(renderer);

        if (gl_context) SDL_GL_DestroyContext(gl_context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        bool running = true;
        SDL_Event event;
        auto last_time = std::chrono::high_resolution_clock::now();

        while (running) {
            // Calculate delta time
            auto current_time = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            // Handle events
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                handle_event(event);
            }

            // Update
            nvgcssUpdate(renderer, dt);

            // Render
            int win_width, win_height;
            SDL_GetWindowSize(window, &win_width, &win_height);
            int fb_width, fb_height;
            SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);
            float pixel_ratio = (float)fb_width / (float)win_width;

            glViewport(0, 0, fb_width, fb_height);
            glClearColor(0.96f, 0.97f, 0.98f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            nvgBeginFrame(vg, win_width, win_height, pixel_ratio);

            // Set viewport for CSS renderer
            nvgcssSetViewport(renderer, (float)win_width, (float)win_height);

            // Render CSS elements
            nvgcssRender(renderer);

            // Draw title
            draw_title(win_width, win_height);

            nvgEndFrame(vg);

            SDL_GL_SwapWindow(window);
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    NVGCSSRenderer* renderer = nullptr;

    NVGCSSElement* hover_button = nullptr;
    NVGCSSElement* active_button = nullptr;
    NVGCSSElement* gradient_box = nullptr;
    NVGCSSElement* transform_box = nullptr;

    void init_sdl() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            loge("SDL_Init failed: {}", SDL_GetError());
            exit(1);
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow(
            "NanoVG CSS Basics Demo",
            1000, 700,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );

        if (!window) {
            loge("SDL_CreateWindow failed: {}", SDL_GetError());
            exit(1);
        }

        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1); // Enable vsync
    }

    void init_nanovg() {
        // Initialize glad (OpenGL function loader)
        if (!gladLoadGL()) {
            loge("Failed to initialize glad");
            exit(1);
        }

        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!vg) {
            loge("Failed to create NanoVG context");
            exit(1);
        }

        // Load fonts
        if (nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf") == -1) {
            loge("Failed to load font 'sans-serif'");
        }
        if (nvgCreateFont(vg, "sans-serif-Bold", "resources/Roboto-Bold.ttf") == -1) {
            loge("Failed to load font 'sans-serif-Bold'");
        }

        renderer = nvgcssCreateRenderer(vg);
        if (!renderer) {
            loge("Failed to create NanoVG CSS renderer");
            exit(1);
        }
    }

    void setup_css() {
        // Load main CSS file
        if (!nvgcssLoadCSSFile(renderer, "styles/nanovg_css_basics.css")) {
            loge("Failed to load styles/nanovg_css_basics.css");
            exit(1);
        }

        // Add example-specific CSS for positioning
        const char* example_css = R"(
            /* Row 1: Interactive buttons */
            #hover-btn {
                x: 50px;
                y: 100px;
                width: 150px;
                height: 50px;
            }

            #active-btn {
                x: 230px;
                y: 100px;
                width: 150px;
                height: 50px;
            }

            /* Row 2: Gradient and transform boxes */
            #grad-box {
                x: 50px;
                y: 200px;
                width: 150px;
                height: 100px;
            }

            #transform-box {
                x: 280px;
                y: 225px;
                width: 150px;
                height: 100px;
            }

            /* Row 3: Card and circle */
            #card {
                x: 420px;
                y: 100px;
                width: 100px;
                height: 100px;
            }

            #circle {
                x: 700px;
                y: 120px;
                width: 80px;
                height: 80px;
            }

            /* Row 4: Color-themed buttons */
            #warning-btn {
                x: 50px;
                y: 400px;
                width: 150px;
                height: 100px;
                background: var(--warning-color);
            }

            #danger-btn {
                x: 230px;
                y: 400px;
                width: 150px;
                height: 100px;
                background: var(--danger-color);
            }

            #success-btn {
                x: 410px;
                y: 400px;
                width: 150px;
                height: 100px;
                background: var(--success-color);
            }
        )";

        if (!nvgcssParseCSS(renderer, example_css)) {
            loge("Failed to parse example CSS");
            exit(1);
        }

        // Set CSS variables (override :root values if needed)
        nvgcssSetVariable(renderer, "--primary-color", "#4a90e2");
        nvgcssSetVariable(renderer, "--success-color", "#5cb85c");
        nvgcssSetVariable(renderer, "--danger-color", "#d9534f");
        nvgcssSetVariable(renderer, "--warning-color", "#f0ad4e");
    }

    void create_elements() {
        // Row 1: Interactive buttons
        hover_button = nvgcssCreateElement(renderer, "hover-btn", "rect");
        nvgcssAddClass(hover_button, "button");
        nvgcssSetText(hover_button, "Hover Me");

        active_button = nvgcssCreateElement(renderer, "active-btn", "rect");
        nvgcssAddClass(active_button, "button");
        nvgcssSetText(active_button, "Click Me");

        // Row 2: Gradient and transform boxes
        gradient_box = nvgcssCreateElement(renderer, "grad-box", "rect");
        nvgcssAddClass(gradient_box, "gradient-box");
        nvgcssSetText(gradient_box, "Gradient");

        transform_box = nvgcssCreateElement(renderer, "transform-box", "rect");
        nvgcssAddClass(transform_box, "gradient-box");
        nvgcssSetText(transform_box, "Transform");

        // Row 3: Card and circle
        NVGCSSElement* card = nvgcssCreateElement(renderer, "card", "rect");
        nvgcssAddClass(card, "card");
        nvgcssSetText(card, "Card");

        NVGCSSElement* circle = nvgcssCreateElement(renderer, "circle", "circle");
        nvgcssAddClass(circle, "circle");

        // Row 4: Color-themed buttons
        NVGCSSElement* warning_btn = nvgcssCreateElement(renderer, "warning-btn", "rect");
        nvgcssAddClass(warning_btn, "button");
        nvgcssSetText(warning_btn, "Warning");

        NVGCSSElement* danger_btn = nvgcssCreateElement(renderer, "danger-btn", "rect");
        nvgcssAddClass(danger_btn, "button");
        nvgcssSetText(danger_btn, "Danger");

        NVGCSSElement* success_btn = nvgcssCreateElement(renderer, "success-btn", "rect");
        nvgcssAddClass(success_btn, "button");
        nvgcssSetText(success_btn, "Success");
    }

    void handle_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            float mx = event.motion.x;
            float my = event.motion.y;
            update_hover_state(mx, my);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float mx = event.button.x;
            float my = event.button.y;
            update_active_state(mx, my, true);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            update_active_state(0, 0, false);
        }
    }

    void update_hover_state(float mx, float my) {
        // Update hover states based on mouse position
        check_element_hover(hover_button, mx, my);
        check_element_hover(active_button, mx, my);
        check_element_hover(gradient_box, mx, my);
        check_element_hover(transform_box, mx, my);
    }

    void check_element_hover(NVGCSSElement* elem, float mx, float my) {
        if (!elem) return;

        float x = elem->computed.x;
        float y = elem->computed.y;
        float w = elem->computed.width;
        float h = elem->computed.height;

        bool is_over = (mx >= x && mx <= x + w && my >= y && my <= y + h);
        nvgcssSetPseudoState(elem, "hover", is_over ? 1 : 0);
    }

    void update_active_state(float mx, float my, bool pressed) {
        if (pressed) {
            check_element_active(hover_button, mx, my);
            check_element_active(active_button, mx, my);
        } else {
            nvgcssSetPseudoState(hover_button, "active", 0);
            nvgcssSetPseudoState(active_button, "active", 0);
        }
    }

    void check_element_active(NVGCSSElement* elem, float mx, float my) {
        if (!elem) return;

        float x = elem->computed.x;
        float y = elem->computed.y;
        float w = elem->computed.width;
        float h = elem->computed.height;

        bool is_over = (mx >= x && mx <= x + w && my >= y && my <= y + h);
        nvgcssSetPseudoState(elem, "active", is_over ? 1 : 0);
    }

    void draw_title(int width, int height) {
        nvgFontFace(vg, "sans");
        nvgFontSize(vg, 24.0f);
        nvgFillColor(vg, nvgRGBA(40, 40, 40, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(vg, 50, 30, "NanoVG CSS Basics Demo", nullptr);

        nvgFontSize(vg, 14.0f);
        nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
        nvgText(vg, 50, 60, "Hover over elements to see CSS transitions and pseudo-states", nullptr);
    }
};

int main(int argc, char** argv) {
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setThreadName("main");

    try {
        NanoVGCSSBasicsDemo demo;
        demo.run();
    }
    catch (const std::exception& e) {
        loge("Exception: {}", e.what());
        return 1;
    }

    return 0;
}
