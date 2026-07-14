/*
 * Flex TUI Demo - Using tango as rendering backend
 *
 * Demonstrates flex scene rendering in terminal with:
 * - Rectangles, circles, text
 * - Simple animation loop
 * - Keyboard input handling
 */

#include <flex/core.h>
#include <backends/renderer.h>
#include <backends/tui/init.h>
#include <tui.h>
#include <cmath>

int main() {
    // Initialize tango terminal
    tui_terminal_t* term = tui_terminal_create();
    if (!tui_terminal_init(term)) {
        return 1;
    }
    tui_terminal_hide_cursor(term);
    tui_terminal_enable_mouse(term);

    // Register TUI, then create through the backend-neutral factory.
    flex::tui_backend::register_backend();
    auto renderer = flex::create_renderer(static_cast<flex::CanvasHandle>(term));
    if (!renderer) {
        tui_terminal_cleanup(term);
        tui_terminal_destroy(term);
        return 1;
    }

    int width = tui_terminal_width(term);
    int height = tui_terminal_height(term);
    float frame_w = width * 8.0f;
    float frame_h = height * 16.0f;

    float time = 0.0f;
    bool running = true;

    while (running) {
        // Handle input
        tui_event_t event;
        while (tui_terminal_poll(term, &event)) {
            if (event.type == TUI_EVENT_KEY) {
                if (event.key == TUI_KEY_ESCAPE || event.ch == 'q') {
                    running = false;
                }
            } else if (event.type == TUI_EVENT_RESIZE) {
                tui_terminal_query_size(term);
                width = tui_terminal_width(term);
                height = tui_terminal_height(term);
                frame_w = width * 8.0f;
                frame_h = height * 16.0f;
            }
        }

        // Render frame
        renderer->begin_frame(frame_w, frame_h, 1.0f);
        renderer->clear(flex::Color{0.1f, 0.1f, 0.15f, 1.0f});

        // Animated circle
        float cx = frame_w / 2.0f + std::sin(time) * 100.0f;
        float cy = frame_h / 2.0f + std::cos(time * 0.7f) * 50.0f;
        renderer->draw_circle(cx, cy, 40.0f,
            flex::Paint::solid(flex::Color{0.2f, 0.6f, 1.0f, 1.0f}),
            flex::Paint::solid(flex::Color{1.0f, 1.0f, 1.0f, 1.0f}), 2.0f);

        // Static rectangles
        renderer->draw_rect(50, 50, 120, 60, 0,
            flex::Paint::solid(flex::Color{0.8f, 0.2f, 0.3f, 1.0f}),
            flex::Paint::solid(flex::Color{1.0f, 1.0f, 1.0f, 1.0f}), 1.0f);

        renderer->draw_rect(frame_w - 170, 50, 120, 60, 0,
            flex::Paint::solid(flex::Color{0.2f, 0.8f, 0.3f, 1.0f}),
            flex::Paint::solid(flex::Color{1.0f, 1.0f, 1.0f, 1.0f}), 1.0f);

        // Title text
        renderer->draw_text("Flex + Tango TUI Demo", 50, 150, "default", 12, false,
            flex::Color{1.0f, 1.0f, 0.0f, 1.0f});

        renderer->draw_text("Press 'q' or ESC to quit", 50, 180, "default", 12, false,
            flex::Color{0.7f, 0.7f, 0.7f, 1.0f});

        renderer->end_frame();

        // Present
        tui_terminal_render(term);

        // Animate
        time += 0.05f;

        // ~20 FPS
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50000);
#endif
    }

    tui_terminal_cleanup(term);
    tui_terminal_destroy(term);
    return 0;
}
