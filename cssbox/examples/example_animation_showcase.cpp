/*
 * NanoVG CSS Animation Showcase
 *
 * Demonstrates the complete animation system:
 * 1. CSS Transitions - Smooth property changes
 * 2. CSS Keyframes - Complex multi-step animations
 * 3. Easing Functions - Different timing curves
 * 4. Multiple Properties - Simultaneous animations
 * 5. Interactive Triggers - Hover, click, auto-play
 *
 * Controls:
 * - Hover over boxes to trigger transitions
 * - Click boxes to toggle states
 * - Watch automatic animations
 * - ESC to exit
 */

#include <SDL3/SDL.h>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cssbox.h>
#include <fmtlog.h>
#include <iostream>
#include <chrono>

class AnimationShowcase {
public:
    AnimationShowcase() {
        init_sdl();
        init_nanovg();
        setup_scene();
        last_time_ = std::chrono::high_resolution_clock::now();
    }

    ~AnimationShowcase() {
        if (renderer) cssboxDeleteRenderer(renderer);
        if (vg) nvgDeleteGL3(vg);
        if (gl_context) SDL_GL_DestroyContext(gl_context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        std::cout << "=== NanoVG CSS Animation Showcase ===\n";
        std::cout << "1. Hover over boxes to see transitions\n";
        std::cout << "2. Click boxes to toggle animations\n";
        std::cout << "3. Watch automatic keyframe animations\n";
        std::cout << "Press ESC to exit\n\n";

        bool running = true;
        SDL_Event event;

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.type == SDL_EVENT_WINDOW_MINIMIZED ||
                           event.type == SDL_EVENT_WINDOW_HIDDEN ||
                           event.type == SDL_EVENT_WINDOW_OCCLUDED) {
                    window_visible_ = false;
                } else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                           event.type == SDL_EVENT_WINDOW_RESTORED ||
                           event.type == SDL_EVENT_WINDOW_SHOWN) {
                    window_visible_ = true;
                    last_time_ = std::chrono::high_resolution_clock::now();  // Reset to avoid large dt jump
                    cssboxInvalidatePaint(renderer);
                } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    handle_mouse_move(event.motion.x, event.motion.y);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    handle_click(event.button.x, event.button.y);
                }
            }

            // Skip updates when window is not visible
            if (!window_visible_) {
                SDL_Delay(16);  // ~60fps equivalent, low CPU usage
                continue;
            }

            // Calculate delta time
            auto current_time = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time_).count();
            last_time_ = current_time;

            // Update animations (this is where the magic happens!)
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
    bool window_visible_ = true;

    void init_sdl() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow(
            "NanoVG CSS - Animation Showcase",
            1280, 800,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );

        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);  // VSync
    }

    void init_nanovg() {
        gladLoadGL();
        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

        // Load fonts (required for text rendering)
        if (nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf") == -1) {
            std::cerr << "Warning: Could not load font\n";
        }
        if (nvgCreateFont(vg, "sans-serif-Bold", "resources/Roboto-Bold.ttf") == -1) {
            std::cerr << "Warning: Could not load bold font\n";
        }

        renderer = cssboxCreateRenderer(vg);
        cssboxSetViewport(renderer, 1280, 800);
    }

    void setup_scene() {
        const char* css = R"(
            /* ================================================================
             * SECTION 1: CSS TRANSITIONS (Hover Effects)
             * ================================================================ */

            .section-title {
                position: absolute;
                font-size: 18px;
                font-weight: bold;
                color: #2c3e50;
            }

            /* Row 1: Basic Transitions */
            #title1 { top: 20px; left: 50px; }

            /* Color Transition */
            .color-box {
                position: absolute;
                top: 60px;
                left: 50px;
                width: 100px;
                height: 100px;
                background: #3498db;
                border-radius: 12px;
                transition: background-color 0.3s ease-in-out;
            }
            .color-box:hover {
                background: #e74c3c;
            }

            /* Scale Transition */
            .scale-box {
                position: absolute;
                top: 60px;
                left: 180px;
                width: 100px;
                height: 100px;
                background: #2ecc71;
                border-radius: 12px;
                transform: scale(1);
                transition: transform 0.3s cubic-bezier(0.68, -0.55, 0.265, 1.55);
            }
            .scale-box:hover {
                transform: scale(1.2);
            }

            /* Rotate Transition */
            .rotate-box {
                position: absolute;
                top: 60px;
                left: 310px;
                width: 100px;
                height: 100px;
                background: #f39c12;
                border-radius: 12px;
                transform: rotate(0deg);
                transition: transform 0.4s ease-in-out;
            }
            .rotate-box:hover {
                transform: rotate(45deg);
            }

            /* Opacity Transition */
            .fade-box {
                position: absolute;
                top: 60px;
                left: 440px;
                width: 100px;
                height: 100px;
                background: #9b59b6;
                border-radius: 12px;
                opacity: 0.5;
                transition: opacity 0.3s linear;
            }
            .fade-box:hover {
                opacity: 1.0;
            }

            /* Multi-Property Transition */
            .multi-box {
                position: absolute;
                top: 60px;
                left: 570px;
                width: 100px;
                height: 100px;
                background: #1abc9c;
                border-radius: 12px;
                opacity: 0.8;
                transform: scale(1) rotate(0deg);
                transition: background 0.3s ease,
                           transform 0.3s ease,
                           opacity 0.3s ease,
                           border-radius 0.3s ease;
            }
            .multi-box:hover {
                background: #e67e22;
                transform: scale(1.15) rotate(-10deg);
                opacity: 1.0;
                border-radius: 50%;
            }

            /* ================================================================
             * SECTION 2: CSS KEYFRAME ANIMATIONS (Auto-play)
             * ================================================================ */

            #title2 { top: 200px; left: 50px; }

            /* Spinning Loader */
            @keyframes spin {
                from { transform: rotate(0deg); }
                to   { transform: rotate(360deg); }
            }

            .spinner {
                position: absolute;
                top: 240px;
                left: 75px;
                width: 60px;
                height: 60px;
                /* TEMPORARY: Use solid background instead of border for visibility */
                background: #3498db;
                /* border: 6px solid #ecf0f1; */
                /* border-top-color: #3498db; */
                border-radius: 50%;
                animation: spin 1s linear infinite;
            }

            /* Pulsing Heart */
            @keyframes pulse {
                0%, 100% { transform: scale(1); opacity: 1; }
                50%      { transform: scale(1.3); opacity: 0.7; }
            }

            .pulse-box {
                position: absolute;
                top: 240px;
                left: 205px;
                width: 60px;
                height: 60px;
                background: #e74c3c;
                border-radius: 50%;
                animation: pulse 1.5s ease-in-out infinite;
            }

            /* Bouncing Ball */
            @keyframes bounce {
                0%, 100% { transform: translateY(0); }
                50%      { transform: translateY(-40px); }
            }

            .bounce-box {
                position: absolute;
                top: 270px;
                left: 335px;
                width: 60px;
                height: 60px;
                background: #f39c12;
                border-radius: 50%;
                animation: bounce 0.8s ease-in-out infinite;
            }

            /* Fade In/Out */
            @keyframes fadeInOut {
                0%, 100% { opacity: 0.3; }
                50%      { opacity: 1.0; }
            }

            .fade-anim-box {
                position: absolute;
                top: 240px;
                left: 465px;
                width: 60px;
                height: 60px;
                background: #9b59b6;
                border-radius: 12px;
                animation: fadeInOut 2s ease-in-out infinite;
            }

            /* Color Cycle */
            @keyframes colorCycle {
                0%   { background: #3498db; }
                33%  { background: #e74c3c; }
                66%  { background: #2ecc71; }
                100% { background: #3498db; }
            }

            .color-cycle-box {
                position: absolute;
                top: 240px;
                left: 595px;
                width: 60px;
                height: 60px;
                border-radius: 12px;
                animation: colorCycle 3s linear infinite;
            }

            /* ================================================================
             * SECTION 3: COMPLEX ANIMATIONS (Combinations)
             * ================================================================ */

            #title3 { top: 360px; left: 50px; }

            /* Slide In from Left */
            @keyframes slideInLeft {
                from { transform: translateX(-100px); opacity: 0; }
                to   { transform: translateX(0); opacity: 1; }
            }

            .slide-box {
                position: absolute;
                top: 400px;
                left: 50px;
                width: 150px;
                height: 80px;
                background: #34495e;
                border-radius: 8px;
                animation: slideInLeft 2s ease-out infinite;
            }

            /* Shake Effect */
            @keyframes shake {
                0%, 100% { transform: translateX(0); }
                10%, 30%, 50%, 70%, 90% { transform: translateX(-5px); }
                20%, 40%, 60%, 80% { transform: translateX(5px); }
            }

            .shake-box {
                position: absolute;
                top: 400px;
                left: 230px;
                width: 150px;
                height: 80px;
                background: #e67e22;
                border-radius: 8px;
                transition: background 0.3s;
            }
            .shake-box:hover {
                background: #d35400;
                animation: shake 0.5s ease-in-out;
            }

            /* Flip Animation */
            @keyframes flip {
                0%   { transform: rotateY(0deg); }
                50%  { transform: rotateY(180deg); }
                100% { transform: rotateY(360deg); }
            }

            .flip-box {
                position: absolute;
                top: 400px;
                left: 410px;
                width: 80px;
                height: 80px;
                background: #16a085;
                border-radius: 8px;
                animation: flip 3s ease-in-out infinite;
            }

            /* Growing Circle */
            @keyframes grow {
                0%   { transform: scale(0.5); opacity: 0; }
                50%  { transform: scale(1.2); opacity: 1; }
                100% { transform: scale(1); opacity: 0.8; }
            }

            .grow-box {
                position: absolute;
                top: 420px;
                left: 540px;
                width: 60px;
                height: 60px;
                background: #c0392b;
                border-radius: 50%;
                animation: grow 2s ease-in-out infinite;
            }

            /* ================================================================
             * SECTION 4: LABELS
             * ================================================================ */

            .label {
                position: absolute;
                font-size: 12px;
                color: #7f8c8d;
                text-align: center;
            }

            #label1 { top: 170px; left: 50px; width: 100px; }
            #label2 { top: 170px; left: 180px; width: 100px; }
            #label3 { top: 170px; left: 310px; width: 100px; }
            #label4 { top: 170px; left: 440px; width: 100px; }
            #label5 { top: 170px; left: 570px; width: 100px; }

            #label6 { top: 310px; left: 50px; width: 80px; }
            #label7 { top: 310px; left: 180px; width: 80px; }
            #label8 { top: 310px; left: 310px; width: 80px; }
            #label9 { top: 310px; left: 440px; width: 80px; }
            #label10 { top: 310px; left: 570px; width: 80px; }

            #label11 { top: 490px; left: 50px; width: 150px; }
            #label12 { top: 490px; left: 230px; width: 150px; }
            #label13 { top: 490px; left: 410px; width: 80px; }
            #label14 { top: 490px; left: 520px; width: 80px; }

            /* Performance Stats */
            #fps-label {
                position: absolute;
                top: 750px;
                left: 1100px;
                font-size: 14px;
                color: #27ae60;
                font-weight: bold;
            }
        )";

        cssboxParseCSS(renderer, css);

        // Section Titles
        create_label("title1", "CSS Transitions (Hover Effects)");
        create_label("title2", "CSS Keyframe Animations (Auto-play)");
        create_label("title3", "Complex Animations");

        // Row 1: Transition Boxes
        create_box("color-box", "color-box");
        create_box("scale-box", "scale-box");
        create_box("rotate-box", "rotate-box");
        create_box("fade-box", "fade-box");
        create_box("multi-box", "multi-box");

        // Row 1 Labels
        create_label("label1", "Color\nTransition");
        create_label("label2", "Scale\nTransition");
        create_label("label3", "Rotate\nTransition");
        create_label("label4", "Fade\nTransition");
        create_label("label5", "Multi-Prop\nTransition");

        // Row 2: Keyframe Animations
        create_box("spinner", "spinner");
        create_box("pulse-box", "pulse-box");
        create_box("bounce-box", "bounce-box");
        create_box("fade-anim-box", "fade-anim-box");
        create_box("color-cycle-box", "color-cycle-box");

        // Row 2 Labels
        create_label("label6", "Spinner");
        create_label("label7", "Pulse");
        create_label("label8", "Bounce");
        create_label("label9", "Fade");
        create_label("label10", "Color Cycle");

        // Row 3: Complex Animations
        create_box("slide-box", "slide-box");
        create_box("shake-box", "shake-box");
        create_box("flip-box", "flip-box");
        create_box("grow-box", "grow-box");

        // Row 3 Labels
        create_label("label11", "Slide In");
        create_label("label12", "Shake (Hover)");
        create_label("label13", "Flip");
        create_label("label14", "Grow");

        // FPS Counter
        create_label("fps-label", "60 FPS");
    }

    void create_box(const char* id, const char* css_class) {
        auto* box = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(box, css_class);
    }

    void create_label(const char* id, const char* text) {
        auto* label = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(label, id[0] == 't' ? "section-title" : "label");
        cssboxSetText(label, text);
    }

    void handle_mouse_move(int x, int y) {
        // Section 1: Transition Boxes (Row 1)
        check_hover("color-box", x, y, 50, 60, 100, 100);
        check_hover("scale-box", x, y, 180, 60, 100, 100);
        check_hover("rotate-box", x, y, 310, 60, 100, 100);
        check_hover("fade-box", x, y, 440, 60, 100, 100);
        check_hover("multi-box", x, y, 570, 60, 100, 100);

        // Section 3: Complex Animations (Row 3)
        check_hover("shake-box", x, y, 230, 400, 150, 80);
    }

    void check_hover(const char* id, int mx, int my, float x, float y, float w, float h) {
        cssboxElement* elem = cssboxGetElement(renderer, id);
        if (!elem) return;

        bool is_inside = (mx >= x && mx <= x + w && my >= y && my <= y + h);
        cssboxSetPseudoStateEx(renderer, elem, "hover", is_inside ? 1 : 0);
    }

    void handle_click(int x, int y) {
        // Not used in this demo
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
        glClearColor(0.95f, 0.96f, 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxRender(renderer);
        nvgEndFrame(vg);

        return true;
    }
};

int main() {
    // Disable verbose INFO logs for clean output
    fmtlog::setLogLevel(fmtlog::WRN);

    try {
        AnimationShowcase showcase;
        showcase.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
