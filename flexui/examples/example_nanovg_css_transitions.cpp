/*
 * FlexUI Example - NanoVG CSS Transitions & Animations
 *
 * Demonstrates advanced animation features:
 * - CSS transitions with different easing functions
 * - Multiple property transitions
 * - Chained animations
 * - Transform animations
 * - Color transitions
 * - Interactive state changes
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
#include <fmtlog.h>
#include <chrono>



class NanoVGCSSTransitionsDemo {
public:
    NanoVGCSSTransitionsDemo() {
        init_sdl();
        init_nanovg();
        setup_css();
        create_elements();
    }

    ~NanoVGCSSTransitionsDemo() {
        if (renderer) nvgcssDeleteRenderer(renderer);
         if (gl_context) SDL_GL_DestroyContext(gl_context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        bool running = true;
        SDL_Event event;
        auto last_time = std::chrono::high_resolution_clock::now();
        float animation_time = 0.0f;

        while (running) {
            // Calculate delta time
            auto current_time = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;
            animation_time += dt;

            // Handle events
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                handle_event(event);
            }

            // Animate auto-animated elements
            animate_elements(animation_time);

            // Update transitions
            nvgcssUpdate(renderer, dt);

            // Render
            render();
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    NVGCSSRenderer* renderer = nullptr;

    // Interactive elements
    NVGCSSElement* linear_box = nullptr;
    NVGCSSElement* ease_box = nullptr;
    NVGCSSElement* ease_in_box = nullptr;
    NVGCSSElement* ease_out_box = nullptr;
    NVGCSSElement* ease_in_out_box = nullptr;

    // Auto-animated elements
    NVGCSSElement* pulse_circle = nullptr;
    NVGCSSElement* rotate_square = nullptr;
    NVGCSSElement* color_morph = nullptr;

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
            "NanoVG CSS Transitions Demo",
            1200, 800,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );

        if (!window) {
            loge("SDL_CreateWindow failed: {}", SDL_GetError());
            exit(1);
        }

        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);
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
        const char* css = R"(
            /* Base box styling */
            .ease-box {
                width: 120px;
                height: 120px;
                border-radius: 10px;
                box-shadow: 0 2px 8px rgba(0, 0, 0, 0.15);
            }

            /* Linear easing */
            .linear {
                background: #3498db;
                transition: all 0.8s linear;
            }
            .linear:hover {
                transform: translateX(200px);
                background: #2980b9;
            }

            /* Ease easing */
            .ease {
                background: #9b59b6;
                transition: all 0.8s ease;
            }
            .ease:hover {
                transform: translateX(200px);
                background: #8e44ad;
            }

            /* Ease-in easing */
            .ease-in {
                background: #e74c3c;
                transition: all 0.8s ease-in;
            }
            .ease-in:hover {
                transform: translateX(200px);
                background: #c0392b;
            }

            /* Ease-out easing */
            .ease-out {
                background: #2ecc71;
                transition: all 0.8s ease-out;
            }
            .ease-out:hover {
                transform: translateX(200px);
                background: #27ae60;
            }

            /* Ease-in-out easing */
            .ease-in-out {
                background: #f39c12;
                transition: all 0.8s ease-in-out;
            }
            .ease-in-out:hover {
                transform: translateX(200px);
                background: #d68910;
            }

            /* Pulse animation */
            .pulse {
                width: 100px;
                height: 100px;
                background: radial-gradient(circle, #ff6b6b, #ee5a6f);
                border-radius: 50px;
                transition: transform 0.6s ease-in-out, opacity 0.6s ease-in-out;
            }

            /* Rotate animation */
            .rotate {
                width: 100px;
                height: 100px;
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                border-radius: 10px;
                transition: transform 1.0s ease-in-out;
            }

            /* Color morph */
            .color-morph {
                width: 120px;
                height: 120px;
                border-radius: 60px;
                transition: background 1.5s ease-in-out;
            }

            /* Multi-property transition */
            .card {
                width: 200px;
                height: 150px;
                background: white;
                border: 2px solid #ecf0f1;
                border-radius: 8px;
                box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
                opacity: 1.0;
                transition: all 0.4s ease;
            }

            .card:hover {
                transform: translateY(-10px) scale(1.05);
                box-shadow: 0 8px 24px rgba(0, 0, 0, 0.2);
                border-color: #3498db;
                opacity: 0.95;
            }
        )";

        if (!nvgcssParseCSS(renderer, css)) {
            loge("Failed to parse CSS");
        }
    }

    void create_elements() {
        // Section 1: Easing functions demonstration
        float start_x = 50.0f;
        float start_y = 120.0f;
        float spacing = 140.0f;

        linear_box = nvgcssCreateElement(renderer, "linear-box", "rect");
        nvgcssAddClass(linear_box, "ease-box");
        nvgcssAddClass(linear_box, "linear");
        nvgcssSetStyle(linear_box, "x", std::to_string(start_x).c_str());
        nvgcssSetStyle(linear_box, "y", std::to_string(start_y).c_str());

        ease_box = nvgcssCreateElement(renderer, "ease-box", "rect");
        nvgcssAddClass(ease_box, "ease-box");
        nvgcssAddClass(ease_box, "ease");
        nvgcssSetStyle(ease_box, "x", std::to_string(start_x).c_str());
        nvgcssSetStyle(ease_box, "y", std::to_string(start_y + spacing).c_str());

        ease_in_box = nvgcssCreateElement(renderer, "ease-in-box", "rect");
        nvgcssAddClass(ease_in_box, "ease-box");
        nvgcssAddClass(ease_in_box, "ease-in");
        nvgcssSetStyle(ease_in_box, "x", std::to_string(start_x).c_str());
        nvgcssSetStyle(ease_in_box, "y", std::to_string(start_y + spacing * 2).c_str());

        ease_out_box = nvgcssCreateElement(renderer, "ease-out-box", "rect");
        nvgcssAddClass(ease_out_box, "ease-box");
        nvgcssAddClass(ease_out_box, "ease-out");
        nvgcssSetStyle(ease_out_box, "x", std::to_string(start_x).c_str());
        nvgcssSetStyle(ease_out_box, "y", std::to_string(start_y + spacing * 3).c_str());

        ease_in_out_box = nvgcssCreateElement(renderer, "ease-in-out-box", "rect");
        nvgcssAddClass(ease_in_out_box, "ease-box");
        nvgcssAddClass(ease_in_out_box, "ease-in-out");
        nvgcssSetStyle(ease_in_out_box, "x", std::to_string(start_x).c_str());
        nvgcssSetStyle(ease_in_out_box, "y", std::to_string(start_y + spacing * 4).c_str());

        // Section 2: Auto-animated elements
        pulse_circle = nvgcssCreateElement(renderer, "pulse", "circle");
        nvgcssAddClass(pulse_circle, "pulse");
        nvgcssSetStyle(pulse_circle, "x", "700px");
        nvgcssSetStyle(pulse_circle, "y", "120px");

        rotate_square = nvgcssCreateElement(renderer, "rotate", "rect");
        nvgcssAddClass(rotate_square, "rotate");
        nvgcssSetStyle(rotate_square, "x", "850px");
        nvgcssSetStyle(rotate_square, "y", "120px");

        color_morph = nvgcssCreateElement(renderer, "color-morph", "circle");
        nvgcssAddClass(color_morph, "color-morph");
        nvgcssSetStyle(color_morph, "x", "1000px");
        nvgcssSetStyle(color_morph, "y", "120px");
        nvgcssSetStyle(color_morph, "background", "#3498db");

        // Section 3: Multi-property card
        NVGCSSElement* card = nvgcssCreateElement(renderer, "hover-card", "rect");
        nvgcssAddClass(card, "card");
        nvgcssSetStyle(card, "x", "700px");
        nvgcssSetStyle(card, "y", "400px");
    }

    void handle_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            float mx = event.motion.x;
            float my = event.motion.y;
            update_hover_states(mx, my);
        }
    }

    void update_hover_states(float mx, float my) {
        check_element_hover(linear_box, mx, my);
        check_element_hover(ease_box, mx, my);
        check_element_hover(ease_in_box, mx, my);
        check_element_hover(ease_out_box, mx, my);
        check_element_hover(ease_in_out_box, mx, my);
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

    void animate_elements(float time) {
        // Pulse animation (scale + opacity)
        if (pulse_circle) {
            float pulse = (std::sin(time * 2.0f) + 1.0f) * 0.5f; // 0 to 1
            float scale = 1.0f + pulse * 0.3f;
            float opacity = 1.0f - pulse * 0.3f;
            nvgcssSetStyle(pulse_circle, "transform",
                           ("scale(" + std::to_string(scale) + ")").c_str());
            nvgcssSetStyle(pulse_circle, "opacity", std::to_string(opacity).c_str());
        }

        // Continuous rotation
        if (rotate_square) {
            float angle = std::fmod(time * 60.0f, 360.0f); // Rotate 60 deg/sec
            nvgcssSetStyle(rotate_square, "transform",
                           ("rotate(" + std::to_string(angle) + "deg)").c_str());
        }

        // Color morphing
        if (color_morph) {
            float t = (std::sin(time * 1.0f) + 1.0f) * 0.5f; // 0 to 1

            // Morph between colors
            int r = static_cast<int>(52 + (231 - 52) * t);
            int g = static_cast<int>(152 + (76 - 152) * t);
            int b = static_cast<int>(219 + (60 - 219) * t);

            char color[32];
            snprintf(color, sizeof(color), "rgb(%d, %d, %d)", r, g, b);
            nvgcssSetStyle(color_morph, "background", color);
        }
    }

    void render() {
        int win_width, win_height;
        SDL_GetWindowSize(window, &win_width, &win_height);
        int fb_width, fb_height;
        SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);
        float pixel_ratio = (float)fb_width / (float)win_width;

        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.95f, 0.95f, 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_width, win_height, pixel_ratio);

        // Set viewport for CSS renderer
        nvgcssSetViewport(renderer, (float)win_width, (float)win_height);

        // Render CSS elements
        nvgcssRender(renderer);

        // Draw labels and titles
        draw_ui(win_width, win_height);

        nvgEndFrame(vg);

        SDL_GL_SwapWindow(window);
    }

    void draw_ui(int width, int height) {
        nvgFontFace(vg, "sans");

        // Title
        nvgFontSize(vg, 28.0f);
        nvgFillColor(vg, nvgRGBA(40, 40, 40, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(vg, 50, 30, "CSS Transitions & Animations", nullptr);

        // Subtitle
        nvgFontSize(vg, 14.0f);
        nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
        nvgText(vg, 50, 65, "Hover over the boxes on the left to see different easing functions", nullptr);

        // Section labels
        nvgFontSize(vg, 16.0f);
        nvgFillColor(vg, nvgRGBA(60, 60, 60, 255));

        // Easing function labels
        draw_label(400, 150, "linear");
        draw_label(400, 290, "ease");
        draw_label(400, 430, "ease-in");
        draw_label(400, 570, "ease-out");
        draw_label(400, 710, "ease-in-out");

        // Auto-animation section
        nvgFontSize(vg, 18.0f);
        nvgText(vg, 700, 30, "Auto Animations:", nullptr);

        nvgFontSize(vg, 14.0f);
        nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
        draw_label(700, 250, "Pulse");
        draw_label(850, 250, "Rotate");
        draw_label(1000, 250, "Color Morph");

        nvgText(vg, 700, 350, "Multi-property transition (hover):", nullptr);
    }

    void draw_label(float x, float y, const char* text) {
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgText(vg, x, y, text, nullptr);
    }
};

int main(int argc, char** argv) {
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setThreadName("main");

    try {
        NanoVGCSSTransitionsDemo demo;
        demo.run();
    }
    catch (const std::exception& e) {
        loge("Exception: {}", e.what());
        return 1;
    }

    return 0;
}
