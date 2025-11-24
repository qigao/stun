/*
 * FlexUI Example - NanoVG CSS Layouts
 *
 * Demonstrates layout features:
 * - Flexbox layout (row, column, wrap)
 * - Grid layout (template areas, auto-flow)
 * - Alignment and justification
 * - Gap and spacing
 * - Responsive layouts
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



class NanoVGCSSLayoutsDemo {
public:
    NanoVGCSSLayoutsDemo() {
        init_sdl();
        init_nanovg();
        setup_css();
        create_flexbox_example();
        create_grid_example();
    }

    ~NanoVGCSSLayoutsDemo() {
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
            }

            // Update
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
            "NanoVG CSS Layouts Demo",
            1400, 900,
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
            /* ===== Flexbox Container Styles ===== */
            #flex-row-container {
                display: flex;
                flex-direction: row;
                gap: 10px;
                background: #ecf0f1;
                padding: 15px;
                border-radius: 8px;
                left: 50px;
                top: 100px;
                width: 40%;
            }

            #flex-col-container {
                display: flex;
                flex-direction: column;
                gap: 10px;
                background: #ecf0f1;
                padding: 15px;
                border-radius: 8px;
                left: 45%;
                top: 100px;
                width: 140px;
            }

            #flex-wrap-container {
                display: flex;
                flex-direction: row;
                flex-wrap: wrap;
                gap: 10px;
                background: #ecf0f1;
                padding: 15px;
                border-radius: 8px;
                left: 57%;
                top: 100px;
                width: 40%;
            }

            /* ===== Flex Item Styles ===== */
            .flex-item {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                border-radius: 5px;
                padding: 10px;
                color: white;
                font-size: 14px;
                text-align: center;
                transition: all 0.3s ease;
            }

            .flex-item:hover {
                transform: scale(1.05);
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
            }

            .flex-item-grow {
                flex-grow: 1;
            }

            .flex-item-fixed {
                width: 100px;
                height: 80px;
            }

            /* ===== Grid Container Styles ===== */
            #grid-container {
                display: grid;
                grid-template-columns: repeat(3, 1fr);
                grid-auto-rows: 100px;
                grid-gap: 15px;
                background: #ecf0f1;
                padding: 20px;
                border-radius: 8px;
                left: 50px;
                top: 500px;
                width: 40%;
                height: auto;
            }

            #grid-auto-container {
                display: grid;
                grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
                grid-auto-rows: 90px;
                grid-gap: 15px;
                background: #ecf0f1;
                padding: 20px;
                border-radius: 8px;
                left: 45%;
                top: 500px;
                width: 53%;
                height: auto;
            }

            /* ===== Grid Item Styles ===== */
            .grid-item {
                background: linear-gradient(to bottom, #3498db, #2980b9);
                border-radius: 8px;
                padding: 20px;
                color: white;
                font-size: 14px;
                text-align: center;
                transition: all 0.3s ease;
            }

            .grid-item:hover {
                background: linear-gradient(to bottom, #5dade2, #3498db);
                transform: translateY(-5px);
                box-shadow: 0 6px 16px rgba(0, 0, 0, 0.2);
            }

            .grid-item-span-2 {
                grid-column: span 2;
            }

            .grid-item-span-3 {
                grid-column: span 3;
            }

            /* ===== Color Variations ===== */
            .bg-primary {
                background: linear-gradient(135deg, #3498db, #2980b9);
            }

            .bg-success {
                background: linear-gradient(135deg, #2ecc71, #27ae60);
            }

            .bg-warning {
                background: linear-gradient(135deg, #f39c12, #d68910);
            }

            .bg-danger {
                background: linear-gradient(135deg, #e74c3c, #c0392b);
            }

            .bg-info {
                background: linear-gradient(135deg, #1abc9c, #16a085);
            }
        )";

        if (!nvgcssParseCSS(renderer, css)) {
            loge("Failed to parse CSS");
        }
    }

    void create_flexbox_example() {
        // Create flexbox row container
        NVGCSSElement* flex_row_container = nvgcssCreateElement(renderer, "flex-row-container", "div");

        // Create row items and append to container
        for (int i = 0; i < 4; i++) {
            std::string id = "flex-row-item-" + std::to_string(i);
            NVGCSSElement* item = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(item, "flex-item");
            nvgcssAddClass(item, i == 2 ? "flex-item-grow" : "flex-item-fixed");

            // Apply different colors
            const char* color_classes[] = {"bg-primary", "bg-success", "bg-warning", "bg-danger"};
            nvgcssAddClass(item, color_classes[i % 4]);

            // Add text label
            std::string label = "Row " + std::to_string(i + 1);
            nvgcssSetText(item, label.c_str());

            // Append to container
            nvgcssAppendChild(renderer, flex_row_container, item);
        }

        // Create flexbox column container
        NVGCSSElement* flex_col_container = nvgcssCreateElement(renderer, "flex-col-container", "div");

        // Column layout elements
        for (int i = 0; i < 4; i++) {
            std::string id = "flex-col-item-" + std::to_string(i);
            NVGCSSElement* item = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(item, "flex-item");

            const char* color_classes[] = {"bg-info", "bg-primary", "bg-success", "bg-warning"};
            nvgcssAddClass(item, color_classes[i % 4]);

            // Add text label
            std::string label = "Col " + std::to_string(i + 1);
            nvgcssSetText(item, label.c_str());

            // Append to container
            nvgcssAppendChild(renderer, flex_col_container, item);
        }

        // Create flexbox wrap container
        NVGCSSElement* flex_wrap_container = nvgcssCreateElement(renderer, "flex-wrap-container", "div");

        // Wrap layout elements
        for (int i = 0; i < 8; i++) {
            std::string id = "flex-wrap-item-" + std::to_string(i);
            NVGCSSElement* item = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(item, "flex-item");

            const char* color_classes[] = {"bg-primary", "bg-success", "bg-warning", "bg-danger", "bg-info"};
            nvgcssAddClass(item, color_classes[i % 5]);

            // Add text label
            std::string label = "W" + std::to_string(i + 1);
            nvgcssSetText(item, label.c_str());

            // Append to container
            nvgcssAppendChild(renderer, flex_wrap_container, item);
        }
    }

    void create_grid_example() {
        // Create 3-column grid container
        NVGCSSElement* grid_container = nvgcssCreateElement(renderer, "grid-container", "div");

        // Create grid items and append to container
        for (int i = 0; i < 6; i++) {
            std::string id = "grid-item-" + std::to_string(i);
            NVGCSSElement* item = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(item, "grid-item");

            // Make some items span multiple columns
            if (i == 0) {
                nvgcssAddClass(item, "grid-item-span-2");
            } else if (i == 5) {
                nvgcssAddClass(item, "grid-item-span-3");
            }

            const char* color_classes[] = {"bg-primary", "bg-success", "bg-warning", "bg-danger", "bg-info"};
            nvgcssAddClass(item, color_classes[i % 5]);

            // Add text label
            std::string label = "Grid " + std::to_string(i + 1);
            nvgcssSetText(item, label.c_str());

            // Append to grid container
            nvgcssAppendChild(renderer, grid_container, item);
        }

        // Create auto-fill grid container
        NVGCSSElement* grid_auto_container = nvgcssCreateElement(renderer, "grid-auto-container", "div");

        // Auto-fill grid elements
        for (int i = 0; i < 9; i++) {
            std::string id = "grid-auto-item-" + std::to_string(i);
            NVGCSSElement* item = nvgcssCreateElement(renderer, id.c_str(), "rect");
            nvgcssAddClass(item, "grid-item");

            const char* color_classes[] = {"bg-info", "bg-primary", "bg-success", "bg-warning", "bg-danger"};
            nvgcssAddClass(item, color_classes[i % 5]);

            // Add text label
            std::string label = "G" + std::to_string(i + 1);
            nvgcssSetText(item, label.c_str());

            // Append to auto-fill grid container
            nvgcssAppendChild(renderer, grid_auto_container, item);
        }
    }

    void render() {
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
        nvgText(vg, 50, 30, "CSS Layout Systems", nullptr);

        // Subtitle
        nvgFontSize(vg, 14.0f);
        nvgFillColor(vg, nvgRGBA(100, 100, 100, 255));
        nvgText(vg, 50, 65, "Flexbox and Grid layout demonstrations with responsive behavior", nullptr);

        // Section labels
        nvgFontSize(vg, 18.0f);
        nvgFillColor(vg, nvgRGBA(60, 60, 60, 255));

        // Flexbox section
        nvgText(vg, 50, 220, "Flex Row", nullptr);
        nvgText(vg, 700, 220, "Flex Column", nullptr);
        nvgText(vg, 900, 220, "Flex Wrap", nullptr);

        // Grid section
        nvgFontSize(vg, 20.0f);
        nvgText(vg, 50, 470, "Grid Layouts:", nullptr);

        nvgFontSize(vg, 16.0f);
        nvgFillColor(vg, nvgRGBA(80, 80, 80, 255));
        nvgText(vg, 50, 860, "3-Column Grid (with spanning)", nullptr);
        nvgText(vg, 700, 860, "Auto-fill Responsive Grid", nullptr);
    }
};

int main(int argc, char** argv) {
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setThreadName("main");

    try {
        NanoVGCSSLayoutsDemo demo;
        demo.run();
    }
    catch (const std::exception& e) {
        loge("Exception: {}", e.what());
        return 1;
    }

    return 0;
}
