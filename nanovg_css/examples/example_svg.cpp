/*
 * NanoVG CSS Example - CSS Box Model Demo
 *
 * Demonstrates CSS rendering capabilities using standard box model:
 * - Shapes using border-radius
 * - Backgrounds and gradients
 * - Transitions and hover effects
 * - Positioning and layout
 */

#include <SDL3/SDL.h>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_css.h>
#include <nanovg_css_internal.h>
#include <fmtlog.h>

class CSSDemo {
public:
    CSSDemo() {
        init_sdl();
        init_nanovg();
        setup_scene();
    }

    ~CSSDemo() {
        if (renderer) nvgcssDeleteRenderer(renderer);
        if (vg) nvgDeleteGL3(vg);
        if (gl_context) SDL_GL_DestroyContext(gl_context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        bool running = true;
        SDL_Event event;
        Uint64 last_time = SDL_GetTicks();

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    mouse_x = event.motion.x;
                    mouse_y = event.motion.y;
                }
            }

            Uint64 current_time = SDL_GetTicks();
            double delta_time = (current_time - last_time) / 1000.0;
            last_time = current_time;

            update(delta_time);
            render();
            SDL_GL_SwapWindow(window);
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    NVGCSSRenderer* renderer = nullptr;
    float mouse_x = 0, mouse_y = 0;

    void init_sdl() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow("NanoVG CSS - Box Model Demo", 1200, 800,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);
    }

    void init_nanovg() {
        gladLoadGL();
        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        renderer = nvgcssCreateRenderer(vg);
    }

    void setup_scene() {
        const char* css = 
".circle { width: 80px; height: 80px; background: #4a90e2; border: 3px solid #2e5c8a; border-radius: 50%; position: absolute; transition: all 0.3s ease; }\n"
"\n"
".circle:hover { width: 100px; height: 100px; background: #5aa0f2; border-width: 5px; }\n"
"\n"
".rect { width: 80px; height: 80px; background: #2ecc71; border: 2px solid #27ae60; border-radius: 10px; position: absolute; }\n"
"\n"
".gradient-box { width: 100px; height: 80px; background: linear-gradient(to bottom, #667eea, #764ba2); border-radius: 5px; position: absolute; }\n"
"\n"
".label { font-size: 14px; color: #333; position: absolute; }\n"
"\n"
".title { font-size: 24px; font-weight: bold; color: #2c3e50; position: absolute; }\n";

        logi("CSS string length: {}", strlen(css));
        int parse_result = nvgcssParseCSS(renderer, css);
        logi("CSS parsed: {} rules", parse_result);
        logi("Total rules in stylesheet: {}", renderer->stylesheet->get_rules().size());

        // Create circles
        auto circle1 = nvgcssCreateElement(renderer, "circle1", "div");
        nvgcssAddClass(circle1, "circle");
        circle1->inline_style["left"] = "60px";
        circle1->inline_style["top"] = "60px";

        auto circle2 = nvgcssCreateElement(renderer, "circle2", "div");
        nvgcssAddClass(circle2, "circle");
        circle2->inline_style["left"] = "200px";
        circle2->inline_style["top"] = "60px";
        circle2->inline_style["background"] = "#e74c3c";
        circle2->inline_style["border-color"] = "#c0392b";

        // Create rectangles
        auto rect1 = nvgcssCreateElement(renderer, "rect1", "div");
        nvgcssAddClass(rect1, "rect");
        rect1->inline_style["left"] = "350px";
        rect1->inline_style["top"] = "60px";

        auto rect2 = nvgcssCreateElement(renderer, "rect2", "div");
        nvgcssAddClass(rect2, "rect");
        rect2->inline_style["left"] = "480px";
        rect2->inline_style["top"] = "60px";
        rect2->inline_style["background"] = "#f39c12";
        rect2->inline_style["border-color"] = "#e67e22";

        // Create gradient box
        auto grad = nvgcssCreateElement(renderer, "grad", "div");
        nvgcssAddClass(grad, "gradient-box");
        grad->inline_style["left"] = "620px";
        grad->inline_style["top"] = "60px";

        // Create title
        auto title = nvgcssCreateElement(renderer, "title", "div");
        nvgcssAddClass(title, "title");
        title->inline_style["left"] = "50px";
        title->inline_style["top"] = "20px";
        nvgcssSetText(title, "NanoVG CSS - Box Model Demo");

        // Create labels
        auto label1 = nvgcssCreateElement(renderer, "l1", "div");
        nvgcssAddClass(label1, "label");
        label1->inline_style["left"] = "65px";
        label1->inline_style["top"] = "160px";
        nvgcssSetText(label1, "Circle (hover me!)");

        auto label2 = nvgcssCreateElement(renderer, "l2", "div");
        nvgcssAddClass(label2, "label");
        label2->inline_style["left"] = "330px";
        label2->inline_style["top"] = "160px";
        nvgcssSetText(label2, "Rounded Rectangle");

        auto label3 = nvgcssCreateElement(renderer, "l3", "div");
        nvgcssAddClass(label3, "label");
        label3->inline_style["left"] = "630px";
        label3->inline_style["top"] = "160px";
        nvgcssSetText(label3, "Gradient Background");

        // Debug: Log all created elements and their classes
        logi("Created {} elements total", 8);
        logi("circle1: classes={}, display={}", circle1->classes.size(), (int)circle1->style.display);
        if (!circle1->classes.empty()) logi("  class[0]={}", circle1->classes[0]);
        logi("circle2: classes={}, display={}", circle2->classes.size(), (int)circle2->style.display);
        logi("rect1: classes={}, display={}", rect1->classes.size(), (int)rect1->style.display);
        logi("rect2: classes={}, display={}", rect2->classes.size(), (int)rect2->style.display);
        logi("grad: classes={}, display={}", grad->classes.size(), (int)grad->style.display);
        logi("title: classes={}, display={}", title->classes.size(), (int)title->style.display);
        logi("label1: classes={}, display={}", label1->classes.size(), (int)label1->style.display);
        logi("label2: classes={}, display={}", label2->classes.size(), (int)label2->style.display);

        // IMPORTANT: Call nvgcssUpdate once to compute initial styles from CSS
        nvgcssUpdate(renderer, 0.0f);
        
        // Now check styles after computation
        logi("After style computation:");
        logi("circle1: display={}, width={}, style.border.radius[0]={}, computed.border_radius[0]={}", 
             (int)circle1->style.display, 
             circle1->style.width.value,
             circle1->style.border.radius[0],
             circle1->computed.border_radius[0]);
        logi("rect1: display={}, width={}, border_radius[0]={}", 
             (int)rect1->style.display, 
             rect1->style.width.value,
             rect1->computed.border_radius[0]);
        
        nvgcssComputeLayout(renderer);
        
        // Debug: Log computed positions after layout
        logi("After layout:");
        logi("circle1: x={}, y={}, w={}, h={}, border_radius={}", 
             circle1->computed.x, circle1->computed.y, circle1->computed.width, circle1->computed.height,
             circle1->computed.border_radius[0]);
        logi("circle2: x={}, y={}, w={}, h={}, border_radius={}", 
             circle2->computed.x, circle2->computed.y, circle2->computed.width, circle2->computed.height,
             circle2->computed.border_radius[0]);
        logi("rect1: x={}, y={}, w={}, h={}, border_radius={}", 
             rect1->computed.x, rect1->computed.y, rect1->computed.width, rect1->computed.height,
             rect1->computed.border_radius[0]);
    }

    void update(double delta_time) {
        // Check hover on circle1
        auto circle1 = nvgcssGetElement(renderer, "circle1");
        if (circle1) {
            float cx = 100, cy = 100, r = 50;
            float dx = mouse_x - cx;
            float dy = mouse_y - cy;
            bool hover = (dx*dx + dy*dy) <= (r*r);
            nvgcssSetPseudoState(circle1, "hover", hover ? 1 : 0);
        }

        nvgcssUpdate(renderer, (float)delta_time);
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
    fmtlog::setLogLevel(fmtlog::INF);
    CSSDemo demo;
    demo.run();
    return 0;
}
