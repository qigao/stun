/*
 * Data Binding Demo
 * Demonstrates reactive data binding using BindingContext
 */

#include <iostream>
#include <sstream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

// ============================================================================
// CounterController - Handles UI business logic and state
// ============================================================================
class CounterController {
public:
    CounterController(flex::Instance::Ptr instance) : instance_(instance) {
        // Find UI elements
        auto* artboard = instance_->artboard();
        counter_value_text_ = artboard->find("counterValue");
        status_text_ = artboard->find("statusText");
        increment_button_ = artboard->find("incrementButton");
        decrement_button_ = artboard->find("decrementButton");
        
        setup_logic();
    }

    void increment() {
        counter_++;
        update_display();
        if (increment_button_) {
            increment_button_->set_scale(1.1f, 1.1f);
        }
    }

    void decrement() {
        counter_--;
        update_display();
        if (decrement_button_) {
            decrement_button_->set_scale(1.1f, 1.1f);
        }
    }

    void reset() {
        counter_ = 0;
        update_display();
        std::cout << "Counter reset\n";
    }

    void update(float dt) {
        // Simple visual feedback: button scale cooldown
        if (increment_button_ && increment_button_->scale_x() > 1.0f) {
            increment_button_->set_scale(1.0f, 1.0f);
        }
        if (decrement_button_ && decrement_button_->scale_x() > 1.0f) {
            decrement_button_->set_scale(1.0f, 1.0f);
        }
    }

private:
    void setup_logic() {
        // Set initial counter value as input
        instance_->set_input("counter", static_cast<float>(counter_));

        // Update UI immediately
        update_display();

        // Connect UI events
        if (increment_button_) {
            increment_button_->on_click([this]() {
                increment();
            });
        }
        if (decrement_button_) {
            decrement_button_->on_click([this]() {
                decrement();
            });
        }
    }

    void update_display() {
        // Update counter value input for data binding and state machine
        instance_->set_input("counter", static_cast<float>(counter_));

        // Note: counter text display is now controlled by state machine animations
        // We update the input value, and the state machine transitions will
        // trigger animations that update both the counter display and status text

        // Update counter value text manually (only the number display)
        if (counter_value_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(counter_value_text_)) {
                text->set_content(std::to_string(counter_));
            }
        }

        // Status text is controlled by state machine - do NOT update manually!

        std::cout << "Counter: " << counter_ << "\n";
    }

    flex::Instance::Ptr instance_;
    flex::Node* counter_value_text_ = nullptr;
    flex::Node* status_text_ = nullptr;
    flex::Node* increment_button_ = nullptr;
    flex::Node* decrement_button_ = nullptr;
    int counter_ = 0;
};

// ============================================================================
// DataBindingDemo - App Shell (SDL, Rendering, Engine management)
// ============================================================================
class DataBindingDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Data Binding Demo - Flex Engine",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH, HEIGHT,
            SDL_WINDOW_SHOWN
        );
        if (!window_) return false;

        sdl_renderer_ = SDL_CreateRenderer(window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdl_renderer_) return false;

        texture_ = SDL_CreateTexture(sdl_renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            WIDTH, HEIGHT);
        if (!texture_) return false;

        if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

        canvas_ = tvg::SwCanvas::gen();
        if (!canvas_) return false;

        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        flex::init();

        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        std::cout << "Loading Data Binding Demo...\n";
        auto definition = flex::Definition::load_file("data_binding.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Initialize UI Logic Controller
        controller_ = std::make_unique<CounterController>(instance_);

        // Debug: Check state machines
        auto* machine = instance_->get_machine("statusTracker");
        if (machine) {
            std::cout << "State machine 'statusTracker' found\n";
            auto* layer = machine->get_layer("status");
            if (layer) {
                std::cout << "  Layer 'status' current state: " << layer->current_state() << "\n";
            }
        } else {
            std::cout << "WARNING: State machine 'statusTracker' NOT FOUND\n";
        }

        std::cout << "Data Binding Demo initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  Click + button to increment\n";
        std::cout << "  Click - button to decrement\n";
        std::cout << "  UP/DOWN arrows also work\n";
        std::cout << "  R to reset\n";
        std::cout << "  ESC to quit\n\n";

        return true;
    }

    void run() {
        running_ = true;
        Uint32 last_time = SDL_GetTicks();

        while (running_) {
            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;

            handle_events();
            update(dt);
            render();
            SDL_Delay(16);  // Limit to ~60 FPS
        }
    }

    ~DataBindingDemo() {
        controller_.reset();
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();

        if (canvas_) delete canvas_;
        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    static constexpr int WIDTH = 400;
    static constexpr int HEIGHT = 300;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
    std::unique_ptr<CounterController> controller_;

    bool running_ = false;

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running_ = false;
                            break;

                        case SDLK_UP:
                            if (controller_) controller_->increment();
                            break;

                        case SDLK_DOWN:
                            if (controller_) controller_->decrement();
                            break;

                        case SDLK_r:
                            if (controller_) controller_->reset();
                            break;
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    instance_->send_pointer_event(static_cast<float>(event.button.x), 
                                                static_cast<float>(event.button.y), true);
                    break;
                case SDL_MOUSEBUTTONUP:
                    instance_->send_pointer_event(static_cast<float>(event.button.x), 
                                                static_cast<float>(event.button.y), false);
                    break;
                case SDL_MOUSEMOTION:
                    instance_->send_pointer_event(static_cast<float>(event.motion.x), 
                                                static_cast<float>(event.motion.y), 
                                                (event.motion.state & SDL_BUTTON_LMASK) != 0);
                    break;
            }
        }
    }


    void update(float dt) {
        if (controller_) controller_->update(dt);
        instance_->advance(dt);
    }

    void render() {
        canvas_->remove();

        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
        flex_renderer_->clear(instance_->artboard()->background());
        instance_->render(*flex_renderer_);
        flex_renderer_->end_frame();

        canvas_->draw();
        canvas_->sync();

        SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));
        SDL_RenderClear(sdl_renderer_);
        SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer_);
    }
};

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "Data Binding Demo - Flex Engine\n";
    std::cout << "===========================================\n\n";

    DataBindingDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
