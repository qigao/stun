#include "flexui/raii/sdl_window.h"
#include <fmtlog.h>
#include <glad/glad.h> // For gladLoadGL()

namespace flexui {
namespace raii {

SDLWindow::SDLWindow(const std::string& title, int width, int height, Uint32 flags)
{
    m_window = SDL_CreateWindow(title.c_str(), width, height, flags | SDL_WINDOW_OPENGL);
    if (!m_window) {
        loge("Failed to create window: %s", SDL_GetError());
        return;
    }

    m_gl_context = SDL_GL_CreateContext(m_window);
    if (!m_gl_context) {
        loge("Failed to create OpenGL context: %s", SDL_GetError());
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return;
    }

    if (!SDL_GL_MakeCurrent(m_window, m_gl_context)) {
        loge("Failed to make GL context current: %s", SDL_GetError());
        SDL_GL_DestroyContext(m_gl_context);
        m_gl_context = nullptr;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return;
    }

    if (!gladLoadGL()) {
        loge("Failed to load OpenGL functions via GLAD");
        SDL_GL_DestroyContext(m_gl_context);
        m_gl_context = nullptr;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return;
    }
}

SDLWindow::~SDLWindow()
{
    if (m_gl_context) {
        SDL_GL_DestroyContext(m_gl_context);
        m_gl_context = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

SDLWindow::SDLWindow(SDLWindow&& other) noexcept
    : m_window(other.m_window), m_gl_context(other.m_gl_context)
{
    other.m_window = nullptr;
    other.m_gl_context = nullptr;
}

SDLWindow& SDLWindow::operator=(SDLWindow&& other) noexcept
{
    if (this != &other) {
        // Release existing resources
        if (m_gl_context) {
            SDL_GL_DestroyContext(m_gl_context);
        }
        if (m_window) {
            SDL_DestroyWindow(m_window);
        }

        // Transfer ownership
        m_window = other.m_window;
        m_gl_context = other.m_gl_context;

        // Invalidate other
        other.m_window = nullptr;
        other.m_gl_context = nullptr;
    }
    return *this;
}

} // namespace raii
} // namespace flexui
