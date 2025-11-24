#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace flexui {
namespace raii {

class SDLWindow {
public:
    SDLWindow(const std::string& title, int width, int height, Uint32 flags);
    ~SDLWindow();

    // Delete copy constructor and assignment operator
    SDLWindow(const SDLWindow&) = delete;
    SDLWindow& operator=(const SDLWindow&) = delete;

    // Move constructor and assignment operator
    SDLWindow(SDLWindow&& other) noexcept;
    SDLWindow& operator=(SDLWindow&& other) noexcept;

    SDL_Window* get() const { return m_window; }
    SDL_GLContext getGLContext() const { return m_gl_context; }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_gl_context = nullptr;
};

} // namespace raii
} // namespace flexui
