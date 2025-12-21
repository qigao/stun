/*
 * SDL Platform Implementation
 *
 * Implements Platform abstraction using SDL2.
 */

#pragma once

#include "platform.h"
#include <SDL2/SDL.h>
#include <vector>

namespace editor {

// SDL Render Target
class SDLRenderTarget : public RenderTarget {
public:
    SDLRenderTarget(int width, int height) {
        surface_ = SDL_CreateRGBSurfaceWithFormat(
            0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    }

    ~SDLRenderTarget() {
        if (surface_) SDL_FreeSurface(surface_);
    }

    int width() const override { return surface_->w; }
    int height() const override { return surface_->h; }
    int stride() const override { return surface_->pitch / 4; }
    uint32_t* pixels() override { return static_cast<uint32_t*>(surface_->pixels); }

    void lock() override { SDL_LockSurface(surface_); }
    void unlock() override { SDL_UnlockSurface(surface_); }

    SDL_Surface* surface() { return surface_; }

private:
    SDL_Surface* surface_ = nullptr;
};

// SDL Window
class SDLWindow : public Window {
public:
    SDLWindow(const std::string& title, int width, int height)
        : width_(width), height_(height) {
        window_ = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);

        texture_ = SDL_CreateTexture(renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height);

        render_target_ = std::make_unique<SDLRenderTarget>(width, height);
    }

    ~SDLWindow() {
        render_target_.reset();
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
    }

    int width() const override { return width_; }
    int height() const override { return height_; }

    void setTitle(const std::string& title) override {
        SDL_SetWindowTitle(window_, title.c_str());
    }

    void setSize(int width, int height) override {
        SDL_SetWindowSize(window_, width, height);
        resize(width, height);
    }

    RenderTarget* renderTarget() override { return render_target_.get(); }

    void present() override {
        // Copy surface to texture
        auto* target = static_cast<SDLRenderTarget*>(render_target_.get());
        SDL_UpdateTexture(texture_, nullptr,
            target->surface()->pixels, target->surface()->pitch);

        // Present
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

    // Event callbacks
    void onMouseDown(MouseCallback cb) override { on_mouse_down_ = cb; }
    void onMouseUp(MouseCallback cb) override { on_mouse_up_ = cb; }
    void onMouseMove(MouseCallback cb) override { on_mouse_move_ = cb; }
    void onMouseWheel(WheelCallback cb) override { on_mouse_wheel_ = cb; }
    void onKeyDown(KeyCallback cb) override { on_key_down_ = cb; }
    void onKeyUp(KeyCallback cb) override { on_key_up_ = cb; }
    void onResize(ResizeCallback cb) override { on_resize_ = cb; }
    void onClose(CloseCallback cb) override { on_close_ = cb; }

    // Process SDL event (called by SDLPlatform)
    void processEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    resize(event.window.data1, event.window.data2);
                    if (on_resize_) {
                        on_resize_({event.window.data1, event.window.data2});
                    }
                } else if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    if (on_close_) on_close_();
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (on_mouse_down_) {
                    on_mouse_down_(makeMouseEvent(event.button, true));
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (on_mouse_up_) {
                    on_mouse_up_(makeMouseEvent(event.button, false));
                }
                break;

            case SDL_MOUSEMOTION:
                if (on_mouse_move_) {
                    PlatformMouseEvent e;
                    e.x = static_cast<float>(event.motion.x);
                    e.y = static_cast<float>(event.motion.y);
                    e.button = -1;
                    e.pressed = false;
                    auto mod = SDL_GetModState();
                    e.shift = (mod & KMOD_SHIFT) != 0;
                    e.ctrl = (mod & KMOD_CTRL) != 0;
                    e.alt = (mod & KMOD_ALT) != 0;
                    on_mouse_move_(e);
                }
                break;

            case SDL_MOUSEWHEEL:
                if (on_mouse_wheel_) {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    on_mouse_wheel_({
                        static_cast<float>(mx), static_cast<float>(my),
                        static_cast<float>(event.wheel.x), static_cast<float>(event.wheel.y)
                    });
                }
                break;

            case SDL_KEYDOWN:
                if (on_key_down_) {
                    on_key_down_(makeKeyEvent(event.key, true));
                }
                break;

            case SDL_KEYUP:
                if (on_key_up_) {
                    on_key_up_(makeKeyEvent(event.key, false));
                }
                break;
        }
    }

private:
    void resize(int width, int height) {
        width_ = width;
        height_ = height;

        // Recreate texture and render target
        if (texture_) SDL_DestroyTexture(texture_);
        texture_ = SDL_CreateTexture(renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height);

        render_target_ = std::make_unique<SDLRenderTarget>(width, height);
    }

    PlatformMouseEvent makeMouseEvent(const SDL_MouseButtonEvent& e, bool pressed) {
        PlatformMouseEvent me;
        me.x = static_cast<float>(e.x);
        me.y = static_cast<float>(e.y);
        me.button = e.button - 1;  // SDL uses 1-based
        me.pressed = pressed;
        auto mod = SDL_GetModState();
        me.shift = (mod & KMOD_SHIFT) != 0;
        me.ctrl = (mod & KMOD_CTRL) != 0;
        me.alt = (mod & KMOD_ALT) != 0;
        return me;
    }

    PlatformKeyEvent makeKeyEvent(const SDL_KeyboardEvent& e, bool pressed) {
        PlatformKeyEvent ke;
        ke.key = e.keysym.sym;
        ke.pressed = pressed;
        ke.shift = (e.keysym.mod & KMOD_SHIFT) != 0;
        ke.ctrl = (e.keysym.mod & KMOD_CTRL) != 0;
        ke.alt = (e.keysym.mod & KMOD_ALT) != 0;
        return ke;
    }

    int width_, height_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::unique_ptr<SDLRenderTarget> render_target_;

    MouseCallback on_mouse_down_;
    MouseCallback on_mouse_up_;
    MouseCallback on_mouse_move_;
    WheelCallback on_mouse_wheel_;
    KeyCallback on_key_down_;
    KeyCallback on_key_up_;
    ResizeCallback on_resize_;
    CloseCallback on_close_;
};

// SDL Platform
class SDLPlatform : public Platform {
public:
    bool init() override {
        return SDL_Init(SDL_INIT_VIDEO) >= 0;
    }

    void shutdown() override {
        SDL_Quit();
    }

    std::unique_ptr<Window> createWindow(
        const std::string& title, int width, int height) override {
        auto window = std::make_unique<SDLWindow>(title, width, height);
        windows_.push_back(window.get());
        return window;
    }

    void run(std::function<void()> frameCallback) override {
        running_ = true;
        SDL_Event event;

        while (running_) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running_ = false;
                    continue;
                }

                // Route to appropriate window
                for (auto* window : windows_) {
                    window->processEvent(event);
                }
            }

            if (running_ && frameCallback) {
                frameCallback();
            }

            SDL_Delay(16);  // ~60 FPS
        }
    }

    void quit() override {
        running_ = false;
    }

    const char* name() const override { return "SDL2"; }

private:
    bool running_ = false;
    std::vector<SDLWindow*> windows_;
};

// Factory implementation
inline std::unique_ptr<Platform> createPlatform() {
    return std::make_unique<SDLPlatform>();
}

} // namespace editor
