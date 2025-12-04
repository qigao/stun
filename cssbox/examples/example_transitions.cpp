/*
 * NanoVG CSS Example - Transitions
 *
 * Demonstrates CSS transitions with timing functions:
 * - Color transitions (background-color)
 * - Transform transitions (scale, rotate)
 * - Opacity transitions
 * - Multiple properties transitioning simultaneously
 * - Different timing functions (linear, ease, ease-in-out)
 * - Different durations (fast vs slow)
 *
 * Validates the transition system works with typed CSS properties.
 */

#include <SDL3/SDL.h>

// Include GLAD for OpenGL function loading
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

// Include NanoVG with GL3 backend
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <cssbox.h>
#include <fmtlog.h>
#include <chrono>
#include <set>

class TransitionsDemo {
public:
    TransitionsDemo() {
        init_sdl();
        init_nanovg();
        setup_scene();
        last_time_ = std::chrono::high_resolution_clock::now();
    }

    ~TransitionsDemo() {
        if (renderer) cssboxDeleteRenderer(renderer);
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
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                           event.type == SDL_EVENT_WINDOW_RESTORED ||
                           event.type == SDL_EVENT_WINDOW_SHOWN) {
                    cssboxInvalidatePaint(renderer);
                }
                handle_event(event);
            }

            // Calculate delta time for transitions
            auto current_time = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time_).count();
            last_time_ = current_time;

            // Update transitions
            cssboxUpdate(renderer, dt);

            if (render()) {
                SDL_GL_SwapWindow(window);
            } else {
                SDL_Delay(1);
            }
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    cssboxRenderer* renderer = nullptr;
    std::chrono::high_resolution_clock::time_point last_time_;

    // Track interactive elements
    cssboxElement* color_box = nullptr;
    cssboxElement* scale_box = nullptr;
    cssboxElement* fade_box = nullptr;
    cssboxElement* rotate_box = nullptr;
    cssboxElement* multi_box = nullptr;
    cssboxElement* position_box = nullptr;
    cssboxElement* fast_box = nullptr;
    cssboxElement* slow_box = nullptr;

    std::set<cssboxElement*> hovered_elements;

    void init_sdl() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow("NanoVG CSS Transitions", 1000, 700,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);
    }

    void init_nanovg() {
        gladLoadGL();
        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        renderer = cssboxCreateRenderer(vg);
    }

    void setup_scene() {
        const char* css = R"(
            /* === Base Box Style === */
            .box {
                width: 120px;
                height: 120px;
                border-radius: 12px;
                position: absolute;
                cursor: pointer;
            }

            /* === Row 1: Simple Property Transitions === */

            /* Color Transition (ease-in-out) */
            #color-box {
                top: 80px;
                left: 50px;
                background: #2196F3;
                transition: background-color 0.3s ease-in-out;
            }
            #color-box:hover {
                background: #F44336;
            }

            /* Scale Transition (ease-in-out) */
            #scale-box {
                top: 80px;
                left: 200px;
                background: #4CAF50;
                transition: transform 0.3s ease-in-out;
            }
            #scale-box:hover {
                transform: scale(1.3);
            }

            /* Opacity Transition (linear) */
            #fade-box {
                top: 80px;
                left: 350px;
                background: #9C27B0;
                opacity: 0.4;
                transition: opacity 0.4s linear;
            }
            #fade-box:hover {
                opacity: 1.0;
            }

            /* Rotate Transition (ease) */
            #rotate-box {
                top: 80px;
                left: 500px;
                background: #FF9800;
                transition: transform 0.4s ease;
            }
            #rotate-box:hover {
                transform: rotate(45deg);
            }

            /* === Row 2: Complex Transitions === */

            /* Multiple Properties */
            #multi-box {
                top: 240px;
                left: 50px;
                background: #00BCD4;
                opacity: 0.7;
                transition: background-color 0.3s ease, opacity 0.3s ease, transform 0.3s ease;
            }
            #multi-box:hover {
                background: #E91E63;
                opacity: 1.0;
                transform: scale(1.2) rotate(10deg);
            }

            /* Position Transition */
            #position-box {
                top: 240px;
                left: 200px;
                background: #673AB7;
                transition: left 0.5s ease-in-out, top 0.5s ease-in-out;
            }
            #position-box:hover {
                left: 250px;
                top: 290px;
            }

            /* Fast Transition */
            #fast-box {
                top: 240px;
                left: 350px;
                background: #FFC107;
                transition: background-color 0.1s linear, transform 0.1s linear;
            }
            #fast-box:hover {
                background: #795548;
                transform: scale(1.15);
            }

            /* Slow Transition */
            #slow-box {
                top: 240px;
                left: 500px;
                background: #607D8B;
                transition: background-color 1.0s ease-in-out, transform 1.0s ease-in-out;
            }
            #slow-box:hover {
                background: #FF5722;
                transform: rotate(180deg) scale(1.1);
            }

            /* === Labels === */
            .label {
                position: absolute;
                color: #333;
                font-size: 12px;
                font-weight: bold;
            }
            #label-section1 { top: 40px; left: 50px; font-size: 16px; }
            #label-section2 { top: 200px; left: 50px; font-size: 16px; }

            #label-color { top: 210px; left: 55px; }
            #label-scale { top: 210px; left: 205px; }
            #label-fade { top: 210px; left: 360px; }
            #label-rotate { top: 210px; left: 505px; }

            #label-multi { top: 370px; left: 55px; }
            #label-position { top: 370px; left: 190px; }
            #label-fast { top: 370px; left: 345px; }
            #label-slow { top: 370px; left: 500px; }

            /* === Instructions === */
            #instructions {
                position: absolute;
                top: 420px;
                left: 50px;
                width: 600px;
                color: #666;
                font-size: 14px;
            }
        )";

        cssboxParseCSS(renderer, css);

        // === Create Row 1: Simple Transitions ===
        color_box = create_box("color-box");
        scale_box = create_box("scale-box");
        fade_box = create_box("fade-box");
        rotate_box = create_box("rotate-box");

        // === Create Row 2: Complex Transitions ===
        multi_box = create_box("multi-box");
        position_box = create_box("position-box");
        fast_box = create_box("fast-box");
        slow_box = create_box("slow-box");

        // === Create Labels ===
        create_label("label-section1", "Simple Property Transitions");
        create_label("label-section2", "Complex Transitions");

        create_label("label-color", "Color\n0.3s ease-in-out");
        create_label("label-scale", "Scale\n0.3s ease-in-out");
        create_label("label-fade", "Opacity\n0.4s linear");
        create_label("label-rotate", "Rotate\n0.4s ease");

        create_label("label-multi", "Multiple\nProperties");
        create_label("label-position", "Position\n0.5s ease-in-out");
        create_label("label-fast", "Fast\n0.1s");
        create_label("label-slow", "Slow\n1.0s");

        create_label("instructions", "Hover over boxes to see transitions. Notice how different timing functions and durations affect the animation feel.");
    }

    cssboxElement* create_box(const char* id) {
        auto* box = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(box, "box");
        return box;
    }

    void create_label(const char* id, const char* text) {
        auto* label = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(label, "label");
        cssboxSetText(label, text);
    }

    void handle_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            float mx = event.motion.x;
            float my = event.motion.y;

            // Update hover states for all interactive boxes
            update_hover(color_box, mx, my, 50, 80, 120, 120);
            update_hover(scale_box, mx, my, 200, 80, 120, 120);
            update_hover(fade_box, mx, my, 350, 80, 120, 120);
            update_hover(rotate_box, mx, my, 500, 80, 120, 120);

            update_hover(multi_box, mx, my, 50, 240, 120, 120);
            update_hover(position_box, mx, my, 200, 240, 120, 120);
            update_hover(fast_box, mx, my, 350, 240, 120, 120);
            update_hover(slow_box, mx, my, 500, 240, 120, 120);
        }
    }

    void update_hover(cssboxElement* elem, float mx, float my, float x, float y, float w, float h) {
        if (!elem) return;

        bool is_inside = (mx >= x && mx <= x + w && my >= y && my <= y + h);

        if (is_inside) {
            cssboxSetPseudoStateEx(renderer, elem, "hover", 1);
            hovered_elements.insert(elem);
        } else {
            cssboxSetPseudoStateEx(renderer, elem, "hover", 0);
            hovered_elements.erase(elem);
        }
    }

    bool render() {
        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        cssboxSetViewport(renderer, (float)win_w, (float)win_h);

        if (!cssboxNeedsPaint(renderer)) {
            return false;
        }

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxRender(renderer);
        nvgEndFrame(vg);

        return true;
    }
};

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);
    TransitionsDemo demo;
    demo.run();
    return 0;
}
