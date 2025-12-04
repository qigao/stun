/*
 * NanoVG CSS Example - Advanced Features Demo
 */

#include <SDL3/SDL.h>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cssbox.h>
#include <cssbox_internal.h>
#include <fmtlog.h>

class AdvancedDemo {
public:
    AdvancedDemo() {
        init_sdl();
        init_nanovg();
        setup_scene();
    }

    ~AdvancedDemo() {
        if (renderer) cssboxDeleteRenderer(renderer);
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
                    update_hover();
                } else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                           event.type == SDL_EVENT_WINDOW_RESTORED ||
                           event.type == SDL_EVENT_WINDOW_SHOWN) {
                    cssboxInvalidatePaint(renderer);
                }
            }

            Uint64 current_time = SDL_GetTicks();
            double delta_time = (current_time - last_time) / 1000.0;
            last_time = current_time;

            update(delta_time);
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
    float mouse_x = 0, mouse_y = 0;

    void init_sdl() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow("NanoVG CSS - Advanced Features", 1200, 800,
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
        const char* css = 
".gradient-box { width: 120px; height: 100px; background: linear-gradient(135deg, #667eea, #764ba2); border-radius: 20px; box-shadow: 0 10px 30px rgba(102, 126, 234, 0.4); position: absolute; transition: all 0.3s ease; }\n"
"\n"
".gradient-box:hover { transform: scale(1.1); box-shadow: 0 15px 40px rgba(102, 126, 234, 0.6); }\n"
"\n"
".shadow-card { width: 200px; height: 150px; background: white; border-radius: 15px; box-shadow: 0 2px 4px rgba(0,0,0,0.1), 0 8px 16px rgba(0,0,0,0.1), 0 16px 32px rgba(0,0,0,0.1); position: absolute; transition: all 0.3s ease; }\n"
"\n"
".shadow-card:hover { transform: translateY(-10px); box-shadow: 0 4px 8px rgba(0,0,0,0.15), 0 16px 32px rgba(0,0,0,0.15), 0 32px 64px rgba(0,0,0,0.15); }\n"
"\n"
".title { font-size: 28px; font-weight: bold; color: #2c3e50; position: absolute; }\n"
"\n"
".section-title { font-size: 18px; font-weight: bold; color: #34495e; position: absolute; }\n";

        logi("CSS string length: {}", strlen(css));
        int parse_result = cssboxParseCSS(renderer, css);
        logi("CSS parsed: {} rules", parse_result);
        logi("Total rules in stylesheet: {}", renderer->stylesheet->get_rules().size());

        // Title
        auto title = cssboxCreateElement(renderer, "title", "div");
        cssboxAddClass(title, "title");
        title->inline_style["left"] = "50px";
        title->inline_style["top"] = "30px";
        cssboxSetText(title, "Advanced CSS Features Demo");

        // Section 1: Complex Gradients
        auto sec1 = cssboxCreateElement(renderer, "sec1", "div");
        cssboxAddClass(sec1, "section-title");
        sec1->inline_style["left"] = "50px";
        sec1->inline_style["top"] = "90px";
        cssboxSetText(sec1, "Complex Gradients (hover to scale)");

        auto grad1 = cssboxCreateElement(renderer, "grad1", "div");
        cssboxAddClass(grad1, "gradient-box");
        grad1->inline_style["left"] = "50px";
        grad1->inline_style["top"] = "130px";

        auto grad2 = cssboxCreateElement(renderer, "grad2", "div");
        cssboxAddClass(grad2, "gradient-box");
        grad2->inline_style["left"] = "200px";
        grad2->inline_style["top"] = "130px";
        grad2->inline_style["background"] = "linear-gradient(45deg, #f093fb, #f5576c)";

        auto grad3 = cssboxCreateElement(renderer, "grad3", "div");
        cssboxAddClass(grad3, "gradient-box");
        grad3->inline_style["left"] = "350px";
        grad3->inline_style["top"] = "130px";
        grad3->inline_style["background"] = "linear-gradient(to right, #4facfe, #00f2fe)";

        auto grad4 = cssboxCreateElement(renderer, "grad4", "div");
        cssboxAddClass(grad4, "gradient-box");
        grad4->inline_style["left"] = "500px";
        grad4->inline_style["top"] = "130px";
        grad4->inline_style["background"] = "linear-gradient(to bottom, #fa709a, #fee140)";

        // Section 2: Multiple Shadows
        auto sec2 = cssboxCreateElement(renderer, "sec2", "div");
        cssboxAddClass(sec2, "section-title");
        sec2->inline_style["left"] = "50px";
        sec2->inline_style["top"] = "280px";
        cssboxSetText(sec2, "Layered Shadows (hover for depth)");

        auto shadow1 = cssboxCreateElement(renderer, "shadow1", "div");
        cssboxAddClass(shadow1, "shadow-card");
        shadow1->inline_style["left"] = "50px";
        shadow1->inline_style["top"] = "320px";

        auto shadow2 = cssboxCreateElement(renderer, "shadow2", "div");
        cssboxAddClass(shadow2, "shadow-card");
        shadow2->inline_style["left"] = "280px";
        shadow2->inline_style["top"] = "320px";

        auto shadow3 = cssboxCreateElement(renderer, "shadow3", "div");
        cssboxAddClass(shadow3, "shadow-card");
        shadow3->inline_style["left"] = "510px";
        shadow3->inline_style["top"] = "320px";

        // Section 3: SVG Shapes
        auto sec3 = cssboxCreateElement(renderer, "sec3", "div");
        cssboxAddClass(sec3, "section-title");
        sec3->inline_style["left"] = "750px";
        sec3->inline_style["top"] = "90px";
        cssboxSetText(sec3, "SVG Shapes");

        // Circle
        auto circle = cssboxCreateElement(renderer, "circle1", "circle");
        circle->inline_style["cx"] = "800";
        circle->inline_style["cy"] = "170";
        circle->inline_style["r"] = "35";
        circle->inline_style["fill"] = "#e74c3c";
        circle->inline_style["stroke"] = "#c0392b";
        circle->inline_style["stroke-width"] = "3";

        // Rectangle
        auto rect = cssboxCreateElement(renderer, "rect1", "rect");
        rect->inline_style["x"] = "870";
        rect->inline_style["y"] = "135";
        rect->inline_style["width"] = "70";
        rect->inline_style["height"] = "70";
        rect->inline_style["fill"] = "#3498db";
        rect->inline_style["stroke"] = "#2980b9";
        rect->inline_style["stroke-width"] = "3";
        rect->inline_style["rx"] = "10";

        // Ellipse
        auto ellipse = cssboxCreateElement(renderer, "ellipse1", "ellipse");
        ellipse->inline_style["cx"] = "1010";
        ellipse->inline_style["cy"] = "170";
        ellipse->inline_style["rx"] = "45";
        ellipse->inline_style["ry"] = "30";
        ellipse->inline_style["fill"] = "#2ecc71";
        ellipse->inline_style["stroke"] = "#27ae60";
        ellipse->inline_style["stroke-width"] = "3";

        // Polygon (Star)
        auto polygon = cssboxCreateElement(renderer, "polygon1", "polygon");
        polygon->inline_style["points"] = "800,250 815,285 855,285 823,308 835,343 800,320 765,343 777,308 745,285 785,285";
        polygon->inline_style["fill"] = "#f39c12";
        polygon->inline_style["stroke"] = "#e67e22";
        polygon->inline_style["stroke-width"] = "2";

        // Polyline (Zigzag)
        auto polyline = cssboxCreateElement(renderer, "polyline1", "polyline");
        polyline->inline_style["points"] = "880,260 895,290 910,260 925,290 940,260 955,290";
        polyline->inline_style["fill"] = "none";
        polyline->inline_style["stroke"] = "#9b59b6";
        polyline->inline_style["stroke-width"] = "4";

        // Path (Complex curve)
        auto path = cssboxCreateElement(renderer, "path1", "path");
        path->inline_style["d"] = "M 990 275 Q 1010 250 1030 275 T 1070 275";
        path->inline_style["fill"] = "none";
        path->inline_style["stroke"] = "#e91e63";
        path->inline_style["stroke-width"] = "3";

        // Line
        auto line = cssboxCreateElement(renderer, "line1", "line");
        line->inline_style["x1"] = "750";
        line->inline_style["y1"] = "350";
        line->inline_style["x2"] = "1150";
        line->inline_style["y2"] = "350";
        line->inline_style["stroke"] = "#00bcd4";
        line->inline_style["stroke-width"] = "5";

        logi("Created {} elements total", 18);
        logi("grad1: classes={}, display={}", grad1->classes.size(), (int)grad1->style.display);
        if (!grad1->classes.empty()) logi("  class[0]={}", grad1->classes[0]);
        logi("grad2: classes={}, display={}", grad2->classes.size(), (int)grad2->style.display);
        logi("grad3: classes={}, display={}", grad3->classes.size(), (int)grad3->style.display);
        logi("grad4: classes={}, display={}", grad4->classes.size(), (int)grad4->style.display);
        logi("shadow1: classes={}, display={}", shadow1->classes.size(), (int)shadow1->style.display);
        logi("shadow2: classes={}, display={}", shadow2->classes.size(), (int)shadow2->style.display);
        logi("shadow3: classes={}, display={}", shadow3->classes.size(), (int)shadow3->style.display);

        cssboxUpdate(renderer, 0.0f);
        
        logi("After style computation:");
        logi("grad1: display={}, width={}, border_radius={}", 
             (int)grad1->style.display, grad1->style.width.value, grad1->style.border.radius[0]);
        logi("shadow1: display={}, width={}, border_radius={}", 
             (int)shadow1->style.display, shadow1->style.width.value, shadow1->style.border.radius[0]);
        
        cssboxComputeLayout(renderer);

        logi("After layout:");
        logi("grad1: x={}, y={}, w={}, h={}", grad1->layout.x, grad1->layout.y, grad1->layout.width, grad1->layout.height);
        logi("shadow1: x={}, y={}, w={}, h={}", shadow1->layout.x, shadow1->layout.y, shadow1->layout.width, shadow1->layout.height);
    }

    void update_hover() {
        // Check hover on gradient boxes
        for (int i = 1; i <= 4; i++) {
            char id[32];
            snprintf(id, sizeof(id), "grad%d", i);
            auto item = cssboxGetElement(renderer, id);
            if (item) {
                float x = item->layout.x;
                float y = item->layout.y;
                float w = item->layout.width;
                float h = item->layout.height;
                bool hover = (mouse_x >= x && mouse_x <= x + w &&
                             mouse_y >= y && mouse_y <= y + h);
                cssboxSetPseudoStateEx(renderer, item, "hover", hover ? 1 : 0);
            }
        }

        // Check hover on shadow cards
        for (int i = 1; i <= 3; i++) {
            char id[32];
            snprintf(id, sizeof(id), "shadow%d", i);
            auto item = cssboxGetElement(renderer, id);
            if (item) {
                float x = item->layout.x;
                float y = item->layout.y;
                float w = item->layout.width;
                float h = item->layout.height;
                bool hover = (mouse_x >= x && mouse_x <= x + w &&
                             mouse_y >= y && mouse_y <= y + h);
                cssboxSetPseudoStateEx(renderer, item, "hover", hover ? 1 : 0);
            }
        }
    }

    void update(double delta_time) {
        cssboxUpdate(renderer, (float)delta_time);
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
        glClearColor(0.94f, 0.94f, 0.96f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxRender(renderer);
        nvgEndFrame(vg);

        return true;
    }
};

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);
    AdvancedDemo demo;
    demo.run();
    return 0;
}
