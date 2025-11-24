/*
 * FlexUI Example - NanoVG CSS Widgets Showcase
 *
 * Demonstrates various widget types:
 * - Buttons (primary, secondary, danger, success)
 * - Checkboxes and Radio buttons
 * - Text inputs and Text areas
 * - Sliders and Progress bars
 * - Toggles and Switches
 * - Cards and Badges
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
#include "nanovg_css_internal.h"  // For inline_style access
#include <fmtlog.h>
#include <chrono>
#include <string>
#include <vector>

class NanoVGCSSWidgetsShowcase {
public:
    NanoVGCSSWidgetsShowcase() {
        init_sdl();
        init_nanovg();
        setup_css();
        create_button_widgets();
        create_input_widgets();
        create_selection_widgets();
        create_feedback_widgets();
        create_card_widgets();
    }

    ~NanoVGCSSWidgetsShowcase() {
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
            auto current_time = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                handle_event(event);
            }

            // Update animations
            nvgcssUpdate(renderer, dt);

            render();
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    NVGCSSRenderer* renderer = nullptr;

    // Widget state
    bool checkbox_states[3] = {true, false, true};
    int radio_selected = 0;
    bool toggle_state = true;
    float slider_value = 0.6f;

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
            "NanoVG CSS Widgets Showcase",
            1600, 1000,
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
            /* ===== Button Positioning ===== */
            #button-0 { left: 50px; top: 120px; width: 120px; height: 40px; }
            #button-1 { left: 190px; top: 120px; width: 120px; height: 40px; }
            #button-2 { left: 330px; top: 120px; width: 120px; height: 40px; }
            #button-3 { left: 470px; top: 120px; width: 120px; height: 40px; }
            #button-4 { left: 50px; top: 180px; width: 120px; height: 40px; }
            #button-5 { left: 190px; top: 180px; width: 120px; height: 40px; }
            #button-6 { left: 330px; top: 180px; width: 120px; height: 40px; }

            /* ===== Input Positioning ===== */
            #input-0 { left: 50px; top: 280px; width: 250px; height: 40px; }
            #input-1 { left: 330px; top: 280px; width: 250px; height: 40px; }
            #input-2 { left: 610px; top: 280px; width: 250px; height: 40px; }

            /* ===== Slider Positioning ===== */
            #slider-track { left: 50px; top: 360px; width: 300px; }
            #slider-fill { left: 50px; top: 360px; }
            #slider-thumb { left: 230px; top: 354px; }

            /* ===== Selection Widget Positioning ===== */
            #checkbox-0 { left: 450px; top: 360px; }
            #checkbox-1 { left: 490px; top: 360px; }
            #checkbox-2 { left: 530px; top: 360px; }
            #radio-0 { left: 610px; top: 360px; }
            #radio-1 { left: 650px; top: 360px; }
            #radio-2 { left: 690px; top: 360px; }
            #toggle { left: 750px; top: 360px; }
            #toggle-handle { left: 776px; top: 362px; }

            /* ===== Progress Bar Positioning ===== */
            #progress-bg-0, #progress-bar-0 { left: 50px; top: 480px; width: 300px; }
            #progress-bg-1, #progress-bar-1 { left: 50px; top: 510px; width: 300px; }
            #progress-bg-2, #progress-bar-2 { left: 50px; top: 540px; width: 300px; }

            /* ===== Badge Positioning ===== */
            #badge-0 { left: 400px; top: 480px; width: 70px; height: 24px; }
            #badge-1 { left: 485px; top: 480px; width: 70px; height: 24px; }
            #badge-2 { left: 570px; top: 480px; width: 70px; height: 24px; }
            #badge-3 { left: 655px; top: 480px; width: 70px; height: 24px; }
            #badge-4 { left: 740px; top: 480px; width: 70px; height: 24px; }

            /* ===== Card Positioning ===== */
            #card-0 { left: 50px; top: 620px; width: 320px; height: 140px; }
            #card-1 { left: 400px; top: 620px; width: 320px; height: 140px; }
            #card-2 { left: 750px; top: 620px; width: 320px; height: 140px; }

            /* Button styles */
            .button {
                display: inline-block;
                padding: 12px 24px;
                border-radius: 6px;
                color: white;
                font-size: 14px;
                font-weight: 500;
                text-align: center;
                cursor: pointer;
                transition: all 0.3s ease;
                border: none;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
            }

            .button:hover {
                transform: translateY(-2px);
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
            }

            .button-primary {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            }

            .button-secondary {
                background: linear-gradient(135deg, #6c757d 0%, #5a6268 100%);
            }

            .button-success {
                background: linear-gradient(135deg, #28a745 0%, #218838 100%);
            }

            .button-danger {
                background: linear-gradient(135deg, #dc3545 0%, #c82333 100%);
            }

            .button-warning {
                background: linear-gradient(135deg, #ffc107 0%, #e0a800 100%);
                color: #212529;
            }

            .button-info {
                background: linear-gradient(135deg, #17a2b8 0%, #138496 100%);
            }

            .button-outline {
                background: transparent;
                border: 2px solid #667eea;
                color: #667eea;
            }

            /* Input styles */
            .input {
                padding: 10px 15px;
                border: 2px solid #e0e0e0;
                border-radius: 6px;
                background: white;
                font-size: 14px;
                color: #333;
            }

            .input-error {
                border-color: #dc3545;
            }

            .input-success {
                border-color: #28a745;
            }

            /* Checkbox styles */
            .checkbox {
                width: 22px;
                height: 22px;
                border: 2px solid #667eea;
                border-radius: 4px;
                background: white;
                cursor: pointer;
            }

            .checkbox-checked {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                border-color: #667eea;
            }

            /* Radio button styles */
            .radio {
                width: 22px;
                height: 22px;
                border: 2px solid #667eea;
                border-radius: 50%;
                background: white;
                cursor: pointer;
            }

            .radio-checked {
                background: radial-gradient(circle, #667eea 45%, white 45%);
                border-color: #667eea;
            }

            /* Toggle switch styles */
            .toggle {
                width: 52px;
                height: 28px;
                border-radius: 14px;
                background: #ccc;
                cursor: pointer;
            }

            .toggle-on {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            }

            .toggle-handle {
                width: 24px;
                height: 24px;
                border-radius: 12px;
                background: white;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
            }

            /* Slider styles */
            .slider-track {
                width: 300px;
                height: 8px;
                border-radius: 4px;
                background: #e0e0e0;
            }

            .slider-fill {
                height: 8px;
                border-radius: 4px;
                background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
            }

            .slider-thumb {
                width: 20px;
                height: 20px;
                border-radius: 10px;
                background: white;
                border: 3px solid #667eea;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
                cursor: pointer;
            }

            /* Progress bar styles */
            .progress-bg {
                height: 14px;
                border-radius: 7px;
                background: #e8e8e8;
            }

            .progress-bar-1 {
                height: 14px;
                border-radius: 7px;
                background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
            }

            .progress-bar-2 {
                height: 14px;
                border-radius: 7px;
                background: linear-gradient(90deg, #2ecc71 0%, #27ae60 100%);
            }

            .progress-bar-3 {
                height: 14px;
                border-radius: 7px;
                background: linear-gradient(90deg, #f39c12 0%, #d68910 100%);
            }

            /* Badge styles */
            .badge {
                padding: 6px 12px;
                border-radius: 12px;
                font-size: 12px;
                font-weight: 500;
                color: white;
            }

            .badge-primary {
                background: #667eea;
            }

            .badge-success {
                background: #28a745;
            }

            .badge-danger {
                background: #dc3545;
            }

            .badge-warning {
                background: #ffc107;
                color: #212529;
            }

            .badge-info {
                background: #17a2b8;
            }

            /* Card styles */
            .card {
                background: white;
                border-radius: 12px;
                padding: 20px;
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.08);
            }
        )";

        if (!nvgcssParseCSS(renderer, css)) {
            loge("Failed to parse CSS");
        }
    }

    void create_button_widgets() {
        const char* button_types[] = {
            "button-primary", "button-secondary", "button-success",
            "button-danger", "button-warning", "button-info", "button-outline"
        };
        const char* button_labels[] = {
            "Primary", "Secondary", "Success",
            "Danger", "Warning", "Info", "Outline"
        };

        for (int i = 0; i < 7; i++) {
            std::string id = "button-" + std::to_string(i);
            NVGCSSElement* button = nvgcssCreateElement(renderer, id.c_str(), "button");
            nvgcssAddClass(button, "button");
            nvgcssAddClass(button, button_types[i]);

            nvgcssSetText(button, button_labels[i]);
        }
    }

    void create_input_widgets() {
        // Text inputs
        const char* input_states[] = {"", "input-error", "input-success"};
        const char* placeholders[] = {"Normal input", "Error state", "Success state"};

        for (int i = 0; i < 3; i++) {
            std::string id = "input-" + std::to_string(i);
            NVGCSSElement* input = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(input, "input");
            if (input_states[i][0] != '\0') {
                nvgcssAddClass(input, input_states[i]);
            }

            nvgcssSetText(input, placeholders[i]);
        }

        // Slider track
        NVGCSSElement* slider_track = nvgcssCreateElement(renderer, "slider-track", "rect");
        nvgcssAddClass(slider_track, "slider-track");

        // Slider fill
        NVGCSSElement* slider_fill = nvgcssCreateElement(renderer, "slider-fill", "rect");
        nvgcssAddClass(slider_fill, "slider-fill");

        // Slider thumb
        NVGCSSElement* slider_thumb = nvgcssCreateElement(renderer, "slider-thumb", "circle");
        nvgcssAddClass(slider_thumb, "slider-thumb");
    }

    void create_selection_widgets() {
        // Checkboxes
        for (int i = 0; i < 3; i++) {
            std::string id = "checkbox-" + std::to_string(i);
            NVGCSSElement* checkbox = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(checkbox, "checkbox");
            if (checkbox_states[i]) {
                nvgcssAddClass(checkbox, "checkbox-checked");
            }
        }

        // Radio buttons
        for (int i = 0; i < 3; i++) {
            std::string id = "radio-" + std::to_string(i);
            NVGCSSElement* radio = nvgcssCreateElement(renderer, id.c_str(), "circle");
            nvgcssAddClass(radio, "radio");
            if (i == radio_selected) {
                nvgcssAddClass(radio, "radio-checked");
            }
        }

        // Toggle switch
        NVGCSSElement* toggle = nvgcssCreateElement(renderer, "toggle", "rect");
        nvgcssAddClass(toggle, "toggle");
        if (toggle_state) {
            nvgcssAddClass(toggle, "toggle-on");
        }

        // Toggle handle
        NVGCSSElement* handle = nvgcssCreateElement(renderer, "toggle-handle", "circle");
        nvgcssAddClass(handle, "toggle-handle");
    }

    void create_feedback_widgets() {
        // Progress bars
        float progress_values[] = {0.33f, 0.66f, 1.0f};
        const char* progress_classes[] = {"progress-bar-1", "progress-bar-2", "progress-bar-3"};

        for (int i = 0; i < 3; i++) {
            // Background
            std::string bg_id = "progress-bg-" + std::to_string(i);
            NVGCSSElement* bg = nvgcssCreateElement(renderer, bg_id.c_str(), "rect");
            nvgcssAddClass(bg, "progress-bg");

            // Progress bar
            std::string bar_id = "progress-bar-" + std::to_string(i);
            NVGCSSElement* bar = nvgcssCreateElement(renderer, bar_id.c_str(), "rect");
            nvgcssAddClass(bar, progress_classes[i]);
        }

        // Badges
        const char* badge_types[] = {"badge-primary", "badge-success", "badge-danger", "badge-warning", "badge-info"};
        const char* badge_labels[] = {"Primary", "Success", "Danger", "Warning", "Info"};

        for (int i = 0; i < 5; i++) {
            std::string id = "badge-" + std::to_string(i);
            NVGCSSElement* badge = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(badge, "badge");
            nvgcssAddClass(badge, badge_types[i]);

            nvgcssSetText(badge, badge_labels[i]);
        }
    }

    void create_card_widgets() {
        for (int i = 0; i < 3; i++) {
            std::string id = "card-" + std::to_string(i);
            NVGCSSElement* card = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(card, "card");
        }
    }

    void handle_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float mx = event.button.x;
            float my = event.button.y;

            // Check checkbox clicks
            for (int i = 0; i < 3; i++) {
                float x = 450 + i * 40;
                float y = 360;
                if (mx >= x && mx <= x + 22 && my >= y && my <= y + 22) {
                    checkbox_states[i] = !checkbox_states[i];
                    update_checkboxes();
                }
            }

            // Check radio clicks
            for (int i = 0; i < 3; i++) {
                float x = 570 + i * 40;
                float y = 360;
                if (mx >= x && mx <= x + 22 && my >= y && my <= y + 22) {
                    radio_selected = i;
                    update_radios();
                }
            }

            // Check toggle click
            float toggle_x = 690;
            float toggle_y = 360;
            if (mx >= toggle_x && mx <= toggle_x + 52 && my >= toggle_y && my <= toggle_y + 28) {
                toggle_state = !toggle_state;
                update_toggle();
            }
        }
    }

    void update_checkboxes() {
        for (int i = 0; i < 3; i++) {
            std::string id = "checkbox-" + std::to_string(i);
            NVGCSSElement* checkbox = nvgcssGetElement(renderer, id.c_str());
            if (checkbox) {
                if (checkbox_states[i]) {
                    nvgcssAddClass(checkbox, "checkbox-checked");
                } else {
                    nvgcssRemoveClass(checkbox, "checkbox-checked");
                }
            }
        }
    }

    void update_radios() {
        for (int i = 0; i < 3; i++) {
            std::string id = "radio-" + std::to_string(i);
            NVGCSSElement* radio = nvgcssGetElement(renderer, id.c_str());
            if (radio) {
                if (i == radio_selected) {
                    nvgcssAddClass(radio, "radio-checked");
                } else {
                    nvgcssRemoveClass(radio, "radio-checked");
                }
            }
        }
    }

    void update_toggle() {
        NVGCSSElement* toggle = nvgcssGetElement(renderer, "toggle");
        NVGCSSElement* handle = nvgcssGetElement(renderer, "toggle-handle");

        if (toggle) {
            if (toggle_state) {
                nvgcssAddClass(toggle, "toggle-on");
            } else {
                nvgcssRemoveClass(toggle, "toggle-on");
            }
        }

        // NOTE: This uses inline_style for DYNAMIC positioning (based on toggle state)
        // This is a legitimate use case - handle position must change in response to interaction
        if (handle) {
            int handle_x = toggle_state ? 750 + 26 : 750 + 2;
            handle->inline_style["left"] = std::to_string(handle_x) + "px";
        }
    }

    void render() {
        int win_width, win_height;
        SDL_GetWindowSize(window, &win_width, &win_height);
        int fb_width, fb_height;
        SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);
        float pixel_ratio = (float)fb_width / (float)win_width;

        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.95f, 0.96f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_width, win_height, pixel_ratio);

        // Set viewport for CSS renderer
        nvgcssSetViewport(renderer, (float)win_width, (float)win_height);

        // Render CSS elements
        nvgcssRender(renderer);

        // Draw labels and titles using NanoVG directly
        draw_ui(win_width, win_height);

        nvgEndFrame(vg);

        SDL_GL_SwapWindow(window);
    }

    void draw_ui(int width, int height) {
        nvgFontFace(vg, "sans-serif");

        // Main title
        nvgFontSize(vg, 32.0f);
        nvgFillColor(vg, nvgRGBA(30, 30, 30, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(vg, 50, 30, "Widget Showcase", nullptr);

        // Subtitle
        nvgFontSize(vg, 15.0f);
        nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
        nvgText(vg, 50, 70, "Interactive UI components with CSS styling and smooth animations", nullptr);

        // Section labels
        nvgFontSize(vg, 18.0f);
        nvgFillColor(vg, nvgRGBA(50, 50, 50, 255));
        nvgFontFace(vg, "sans-serif-Bold");

        nvgText(vg, 50, 95, "Buttons", nullptr);
        nvgText(vg, 50, 255, "Input Fields", nullptr);
        nvgText(vg, 50, 335, "Slider", nullptr);
        nvgText(vg, 450, 335, "Checkboxes", nullptr);
        nvgText(vg, 570, 335, "Radio Buttons", nullptr);
        nvgText(vg, 690, 335, "Toggle", nullptr);
        nvgText(vg, 50, 455, "Progress Bars", nullptr);
        nvgText(vg, 400, 455, "Badges", nullptr);
        nvgText(vg, 50, 595, "Cards", nullptr);

        // Instructions
        nvgFontFace(vg, "sans-serif");
        nvgFontSize(vg, 13.0f);
        nvgFillColor(vg, nvgRGBA(120, 120, 120, 255));
        nvgText(vg, 450, 390, "Click to toggle", nullptr);
        nvgText(vg, 570, 390, "Click to select", nullptr);
        nvgText(vg, 690, 395, "Click to toggle", nullptr);

        // Progress percentages
        nvgFontSize(vg, 12.0f);
        nvgFillColor(vg, nvgRGBA(80, 80, 80, 255));
        nvgText(vg, 360, 483, "33%", nullptr);
        nvgText(vg, 360, 513, "66%", nullptr);
        nvgText(vg, 360, 543, "100%", nullptr);

        // Card content (drawn with NanoVG directly)
        const char* card_titles[] = {"Feature Card", "Product Card", "User Card"};
        const char* card_content[] = {
            "Beautiful cards with hover",
            "Display your products",
            "User profiles in cards"
        };
        const char* card_desc[] = {
            "effects and smooth transitions.",
            "with elegant styling.",
            "look great this way."
        };

        nvgFontFace(vg, "sans-serif-Bold");
        nvgFontSize(vg, 18.0f);
        nvgFillColor(vg, nvgRGBA(44, 62, 80, 255));

        for (int i = 0; i < 3; i++) {
            float card_x = 50 + i * 350;
            nvgText(vg, card_x + 20, 640, card_titles[i], nullptr);

            nvgFontFace(vg, "sans-serif");
            nvgFontSize(vg, 14.0f);
            nvgFillColor(vg, nvgRGBA(85, 85, 85, 255));
            nvgText(vg, card_x + 20, 670, card_content[i], nullptr);
            nvgText(vg, card_x + 20, 690, card_desc[i], nullptr);
        }
    }
};

int main(int argc, char** argv) {
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setThreadName("main");

    try {
        NanoVGCSSWidgetsShowcase demo;
        demo.run();
    }
    catch (const std::exception& e) {
        loge("Exception: {}", e.what());
        return 1;
    }

    return 0;
}
