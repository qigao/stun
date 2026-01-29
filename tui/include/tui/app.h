/*
 * TUI - Terminal User Interface Library
 * 
 * App: Elm Architecture (TEA) application framework.
 */

#pragma once

#include "terminal.h"
#include "input.h"
#include <functional>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace tui {

template <typename Model, typename Msg>
class App {
public:
    using UpdateFn = std::function<bool(Msg, Model&)>;
    using ViewFn = std::function<void(const Model&, Buffer&)>;
    using EventMapFn = std::function<std::optional<Msg>(const Event&)>;
    
    static App create(Model init_model, UpdateFn update, ViewFn view, EventMapFn event_map) {
        return App(std::move(init_model), std::move(update), std::move(view), std::move(event_map));
    }
    
    void run() {
        if (!term_.init()) return;
        input_.enable_mouse();
        
        bool running = true;
        bool needs_redraw = true;
        
        while (running) {
            // Handle resize
            int old_w = term_.width(), old_h = term_.height();
            term_.query_size();
            if (term_.width() != old_w || term_.height() != old_h) {
                needs_redraw = true;
            }
            
            // Process input
            while (auto event = input_.poll()) {
                if (auto msg = event_map_(*event)) {
                    running = update_(*msg, model_);
                    needs_redraw = true;
                }
            }
            
            // Render
            if (needs_redraw) {
                view_(model_, term_.buffer());
                term_.render();
                needs_redraw = false;
            }
            
            sleep_ms(16);  // ~60 FPS
        }
        
        input_.disable_mouse();
        term_.cleanup();
    }
    
    Model& model() { return model_; }
    const Model& model() const { return model_; }
    Terminal& terminal() { return term_; }

private:
    App(Model model, UpdateFn update, ViewFn view, EventMapFn event_map)
        : model_(std::move(model))
        , update_(std::move(update))
        , view_(std::move(view))
        , event_map_(std::move(event_map))
    {}
    
    static void sleep_ms(int ms) {
#ifdef _WIN32
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }
    
    Model model_;
    UpdateFn update_;
    ViewFn view_;
    EventMapFn event_map_;
    Terminal term_;
    Input input_;
};

} // namespace tui
