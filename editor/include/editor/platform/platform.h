/*
 * Platform Abstraction
 *
 * Abstracts windowing, events, and display from the editor.
 * Implementations: SDL, GLFW, native (Win32/Cocoa/X11)
 */

#pragma once

#include "../core/types.h"
#include <functional>
#include <memory>
#include <string>

namespace editor {

// Forward declarations
class Platform;

// Event types for platform-independent input handling
struct PlatformMouseEvent {
    float x, y;
    int button;        // 0=left, 1=middle, 2=right
    bool pressed;      // true=down, false=up
    bool shift, ctrl, alt;
};

struct PlatformKeyEvent {
    int key;           // Platform-independent key code
    bool pressed;
    bool shift, ctrl, alt;
};

struct PlatformWheelEvent {
    float x, y;        // Mouse position
    float deltaX, deltaY;
};

struct PlatformResizeEvent {
    int width, height;
};

// Event callback types
using MouseCallback = std::function<void(const PlatformMouseEvent&)>;
using KeyCallback = std::function<void(const PlatformKeyEvent&)>;
using WheelCallback = std::function<void(const PlatformWheelEvent&)>;
using ResizeCallback = std::function<void(const PlatformResizeEvent&)>;
using CloseCallback = std::function<void()>;

// Render target - abstract buffer that can be drawn to
class RenderTarget {
public:
    virtual ~RenderTarget() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual int stride() const = 0;  // In pixels

    // Get pixel buffer for software rendering
    virtual uint32_t* pixels() = 0;

    // Lock/unlock for safe access (may be no-op for some implementations)
    virtual void lock() {}
    virtual void unlock() {}
};

// Platform window abstraction
class Window {
public:
    virtual ~Window() = default;

    // Window properties
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void setTitle(const std::string& title) = 0;
    virtual void setSize(int width, int height) = 0;

    // Render target for drawing
    virtual RenderTarget* renderTarget() = 0;

    // Present the render target to screen
    virtual void present() = 0;

    // Event callbacks
    virtual void onMouseDown(MouseCallback cb) = 0;
    virtual void onMouseUp(MouseCallback cb) = 0;
    virtual void onMouseMove(MouseCallback cb) = 0;
    virtual void onMouseWheel(WheelCallback cb) = 0;
    virtual void onKeyDown(KeyCallback cb) = 0;
    virtual void onKeyUp(KeyCallback cb) = 0;
    virtual void onResize(ResizeCallback cb) = 0;
    virtual void onClose(CloseCallback cb) = 0;
};

// Platform abstraction - creates windows and runs event loop
class Platform {
public:
    virtual ~Platform() = default;

    // Initialize platform
    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Create a window
    virtual std::unique_ptr<Window> createWindow(
        const std::string& title, int width, int height) = 0;

    // Run the event loop (blocks until quit)
    virtual void run(std::function<void()> frameCallback) = 0;

    // Request exit from event loop
    virtual void quit() = 0;

    // Platform name for debugging
    virtual const char* name() const = 0;
};

// Factory function - implemented by platform backends
std::unique_ptr<Platform> createPlatform();

} // namespace editor
