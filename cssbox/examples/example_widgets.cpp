/*
 * NanoVG CSS Example - Widgets
 *
 * Demonstrates common UI widgets using CSS:
 * - Buttons (hover, active, disabled states)
 * - Cards (shadows, borders, rounded corners)
 * - Input fields (focus states)
 * - Progress bars
 * - Checkboxes and toggles
 *
 * Validates pseudo-states and visual styling in the typed CSS system.
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
#include <set>
#include <string>

class WidgetsDemo {
public:
    WidgetsDemo() {
        init_sdl();
        init_nanovg();
        setup_scene();
    }

    ~WidgetsDemo() {
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
                }
                handle_event(event);
            }

            if (render()) {
                SDL_GL_SwapWindow(window);
            } else {
                // Nothing to paint - sleep to reduce CPU/GPU usage
                SDL_Delay(1);
            }
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    cssboxRenderer* renderer = nullptr;

    // Track interactive elements
    cssboxElement* btn_primary = nullptr;
    cssboxElement* btn_danger = nullptr;
    cssboxElement* input_field = nullptr;
    cssboxElement* checkbox = nullptr;

    std::set<cssboxElement*> hovered_elements;
    std::set<cssboxElement*> active_elements;
    std::set<cssboxElement*> focused_elements;

    void init_sdl() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow("NanoVG CSS Widgets", 1000, 700,
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

        renderer = cssboxCreateRenderer(vg);
    }

    void setup_scene() {
        const char* css = R"(
            /* === Base Button Style === */
            .button {
                width: 140px;
                height: 50px;
                border-radius: 8px;
                border: 2px solid transparent;
                position: absolute;
                cursor: pointer;
            }
            .button:hover {
                transform: translateY(-2px);
                box-shadow: 0 4px 12px rgba(0,0,0,0.2);
            }
            .button:active {
                transform: translateY(0px);
                box-shadow: 0 2px 4px rgba(0,0,0,0.2);
            }

            /* === Button Variants === */
            #btn-primary {
                top: 50px;
                left: 50px;
                background: #2196F3;
                color: white;
            }
            #btn-danger {
                top: 50px;
                left: 220px;
                background: #F44336;
                color: white;
            }
            #btn-success {
                top: 50px;
                left: 390px;
                background: #4CAF50;
                color: white;
            }
            #btn-disabled {
                top: 50px;
                left: 560px;
                background: #CCCCCC;
                color: #999999;
                cursor: not-allowed;
                opacity: 0.6;
            }

            /* === Cards === */
            .card {
                width: 200px;
                height: 140px;
                background: white;
                color: #333333;
                font-size: 14px;
                border-radius: 12px;
                box-shadow: 0 2px 8px rgba(0,0,0,0.1);
                border: 1px solid #e0e0e0;
                padding: 20px;
                position: absolute;
            }
            .card:hover {
                box-shadow: 0 4px 16px rgba(0,0,0,0.15);
                transform: translateY(-4px);
            }

            #card-1 { top: 150px; left: 50px; }
            #card-2 { top: 150px; left: 280px; }
            #card-3 { top: 150px; left: 510px; }

            /* === Input Fields === */
            .input {
                width: 250px;
                height: 45px;
                background: white;
                color: #666666;
                font-size: 14px;
                border: 2px solid #CCCCCC;
                border-radius: 6px;
                padding: 10px;
                position: absolute;
            }
            .input:hover {
                border-color: #999999;
            }
            .input:focus {
                border-color: #2196F3;
                box-shadow: 0 0 0 3px rgba(33,150,243,0.1);
            }

            #input-1 { top: 340px; left: 50px; }
            #input-2 { top: 340px; left: 330px; }

            /* === Progress Bars === */
            .progress-container {
                width: 300px;
                height: 30px;
                background: #E0E0E0;
                border-radius: 15px;
                position: absolute;
                overflow: hidden;
            }
            .progress-bar {
                height: 100%;
                background: linear-gradient(90deg, #4CAF50, #8BC34A);
                border-radius: 15px;
            }

            #progress-1 { top: 430px; left: 50px; }
            #progress-bar-1 { width: 75%; }

            #progress-2 { top: 480px; left: 50px; }
            #progress-bar-2 { width: 45%; }

            #progress-3 { top: 530px; left: 50px; }
            #progress-bar-3 { width: 90%; }

            /* === Checkboxes === */
            .checkbox {
                width: 24px;
                height: 24px;
                background: white;
                border: 2px solid #CCCCCC;
                border-radius: 4px;
                position: absolute;
            }
            .checkbox:hover {
                border-color: #2196F3;
            }
            .checkbox.checked {
                background: #2196F3;
                border-color: #2196F3;
            }

            #checkbox-1 { top: 430px; left: 400px; }
            #checkbox-2 { top: 470px; left: 400px; }
            #checkbox-3 { top: 510px; left: 400px; }

            /* === Toggle Switches === */
            .toggle {
                width: 60px;
                height: 32px;
                background: #CCCCCC;
                border-radius: 16px;
                position: absolute;
            }
            .toggle.on {
                background: #4CAF50;
            }
            .toggle-knob {
                width: 24px;
                height: 24px;
                background: white;
                border-radius: 12px;
                position: absolute;
                top: 4px;
                left: 4px;
            }
            .toggle.on .toggle-knob {
                left: 32px;
            }

            #toggle-1 { top: 430px; left: 500px; }
            #toggle-2 { top: 480px; left: 500px; }

            /* === Labels === */
            .label {
                position: absolute;
                color: #333;
                font-size: 14px;
                font-weight: bold;
            }
            #label-buttons { top: 20px; left: 50px; }
            #label-cards { top: 120px; left: 50px; }
            #label-inputs { top: 310px; left: 50px; }
            #label-progress { top: 400px; left: 50px; }
            #label-checks { top: 400px; left: 400px; }
            #label-toggles { top: 400px; left: 500px; }
        )";

        cssboxParseCSS(renderer, css);

        // === Create Buttons ===
        btn_primary = create_button("btn-primary", "Primary");
        btn_danger = create_button("btn-danger", "Danger");
        create_button("btn-success", "Success");
        create_button("btn-disabled", "Disabled");

        // === Create Cards ===
        create_card("card-1", "Card Title", "This is card content with shadow.");
        create_card("card-2", "Product Card", "Price: $99.99");
        create_card("card-3", "Info Card", "Hover to see elevation effect.");

        // === Create Input Fields ===
        input_field = create_input("input-1", "Email address");
        create_input("input-2", "Password");

        // === Create Progress Bars ===
        create_progress("progress-1", "progress-bar-1");
        create_progress("progress-2", "progress-bar-2");
        create_progress("progress-3", "progress-bar-3");

        // === Create Checkboxes ===
        checkbox = create_checkbox("checkbox-1", false);
        create_checkbox("checkbox-2", true);
        create_checkbox("checkbox-3", false);

        // === Create Toggle Switches ===
        create_toggle("toggle-1", true);
        create_toggle("toggle-2", false);

        // === Create Labels ===
        create_label("label-buttons", "Buttons");
        create_label("label-cards", "Cards");
        create_label("label-inputs", "Input Fields");
        create_label("label-progress", "Progress Bars");
        create_label("label-checks", "Checkboxes");
        create_label("label-toggles", "Toggles");
    }

    cssboxElement* create_button(const char* id, const char* text) {
        auto* btn = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(btn, "button");
        cssboxSetText(btn, text);
        return btn;
    }

    cssboxElement* create_card(const char* id, const char* title, const char* content) {
        auto* card = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(card, "card");

        // For now, combine title and content into text_content
        // TODO: Add child elements for richer card layouts
        std::string card_text = std::string(title) + "\n\n" + content;
        cssboxSetText(card, card_text.c_str());

        return card;
    }

    cssboxElement* create_input(const char* id, const char* placeholder) {
        auto* input = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(input, "input");
        cssboxSetText(input, placeholder);
        return input;
    }

    void create_progress(const char* container_id, const char* bar_id) {
        auto* container = cssboxCreateElement(renderer, container_id, "div");
        cssboxAddClass(container, "progress-container");

        auto* bar = cssboxCreateElement(renderer, bar_id, "div");
        cssboxAddClass(bar, "progress-bar");
        cssboxAppendChild(renderer, container, bar);
    }

    cssboxElement* create_checkbox(const char* id, bool checked) {
        auto* checkbox = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(checkbox, "checkbox");
        if (checked) cssboxAddClass(checkbox, "checked");
        return checkbox;
    }

    void create_toggle(const char* id, bool on) {
        auto* toggle = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(toggle, "toggle");
        if (on) cssboxAddClass(toggle, "on");

        auto* knob = cssboxCreateElement(renderer, "", "div");
        cssboxAddClass(knob, "toggle-knob");
        cssboxAppendChild(renderer, toggle, knob);
    }

    void create_label(const char* id, const char* text) {
        auto* label = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(label, "label");
        cssboxSetText(label, text);
    }

    void handle_event(const SDL_Event& event) {
        // Window expose: repaint needed when window becomes visible
        if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
            event.type == SDL_EVENT_WINDOW_RESTORED ||
            event.type == SDL_EVENT_WINDOW_SHOWN) {
            cssboxInvalidatePaint(renderer);
        }

        // Keyboard: Press 'M' to print memory profile
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_M) {
                cssboxPrintMemoryProfile(renderer);
            }
        }

        // Simple hover/active tracking for demonstration
        // In real app, would use proper hit testing

        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            // Update hover states
            float mx = event.motion.x;
            float my = event.motion.y;

            // Simple bounds check for buttons (hardcoded for demo)
            update_hover_state(btn_primary, mx, my, 50, 50, 140, 50);
            update_hover_state(btn_danger, mx, my, 220, 50, 140, 50);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            // Set active state (use Ex version to trigger repaint)
            if (is_hovered(btn_primary)) {
                cssboxSetPseudoStateEx(renderer, btn_primary, "active", 1);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            // Clear active state
            cssboxSetPseudoStateEx(renderer, btn_primary, "active", 0);
            cssboxSetPseudoStateEx(renderer, btn_danger, "active", 0);
        }
    }

    void update_hover_state(cssboxElement* elem, float mx, float my,
                           float x, float y, float w, float h) {
        if (!elem) return;

        bool is_inside = (mx >= x && mx <= x + w && my >= y && my <= y + h);

        if (is_inside) {
            // Use Ex version to properly trigger repaint
            cssboxSetPseudoStateEx(renderer, elem, "hover", 1);
            hovered_elements.insert(elem);
        } else {
            cssboxSetPseudoStateEx(renderer, elem, "hover", 0);
            hovered_elements.erase(elem);
        }
    }

    bool is_hovered(cssboxElement* elem) {
        return hovered_elements.count(elem) > 0;
    }

    bool render() {
        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        // Update viewport (only marks dirty if size changed)
        cssboxSetViewport(renderer, (float)win_w, (float)win_h);

        // === Retain Mode: Skip frame if nothing changed ===
        // This is the key optimization for 60fps with near-zero GPU when static
        if (!cssboxNeedsPaint(renderer)) {
            return false;  // Nothing to paint, skip entire frame
        }

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxRender(renderer);
        nvgEndFrame(vg);

        return true;  // Painted
    }
};

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);
    WidgetsDemo demo;
    demo.run();
    return 0;
}
