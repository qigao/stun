/*
 * Minimal test - just one big red box
 */

#include <SDL3/SDL.h>

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <nanovg_css.h>
#include <fmtlog.h>

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);

    // Init SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    auto* window = SDL_CreateWindow("TEST - Big Red Box", 800, 600,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    auto* gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    // Init NanoVG
    gladLoadGL();
    auto* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    auto* renderer = nvgcssCreateRenderer(vg);

    // CSS: One big red box
    const char* css = R"(
        #red-box {
            position: absolute;
            top: 100px;
            left: 100px;
            width: 400px;
            height: 300px;
            background: #FF0000;
            border: 5px solid #000000;
        }
    )";

    nvgcssParseCSS(renderer, css);
    nvgcssCreateElement(renderer, "red-box", "div");

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        int win_w, win_h, fb_w, fb_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // BLACK background
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        nvgcssSetViewport(renderer, (float)win_w, (float)win_h);
        nvgcssRender(renderer);
        nvgEndFrame(vg);

        SDL_GL_SwapWindow(window);
    }

    nvgcssDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
