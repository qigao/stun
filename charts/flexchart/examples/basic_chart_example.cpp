#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include <flex/runtime/instance.h>
#include "flex/backends/thorvg/init.h"
#include "flexchart/flexchart.h"
#include "flexchart/chart_component.h"

using namespace flex;
using namespace flex::chart;

class ChartExample {
public:
    bool init(const char* chart_file) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;

        window_ = SDL_CreateWindow("Flex Chart Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 700, SDL_WINDOW_SHOWN);
        sdl_renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 1000, 700);

        flex::init();
        canvas_ = tvg::SwCanvas::gen();
        buffer_.resize(1000 * 700);
        canvas_->target(buffer_.data(), 1000, 1000, 700, tvg::ColorSpace::ARGB8888);
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        // Parse Chart DSL
        std::ifstream file(chart_file);
        if (!file.is_open()) {
            std::cerr << "Failed to open chart file: " << chart_file << "\n";
            return false;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string dsl = ss.str();

        AstProgram program;
        std::string error;
        if (!parse_chart(dsl.c_str(), &program, error)) {
            std::cerr << "Chart Parse Error: " << error << "\n";
            return false;
        }

        if (program.views.empty()) {
            std::cerr << "No chart view found in DSL\n";
            return false;
        }
        auto chart_ast = std::dynamic_pointer_cast<AstChart>(program.views[0]);

        // Create Instance and Scene
        instance_ = Instance::create(1000, 700);
        auto* scene = instance_->scene();
        scene->set_background(Color(0.95f, 0.95f, 0.97f)); // Light gray background

        // Build Chart Node Tree onto Scene Root
        auto* chart_node = ChartComponent::build(chart_ast, *instance_);
        if (chart_node) {
            chart_node->set_layout_size(800, 500);
            chart_node->set_position(100, 100);
            scene->root()->add_child(chart_node);
        }

        // Run Flex Layout
        scene->root()->perform_layout();

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);
        return true;
    }

    void run() {
        bool running = true;
        SDL_Event event;
        Uint32 last_time = SDL_GetTicks();
        while (running) {
            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;
            
            // Clamp dt to avoid huge jumps
            dt = (std::min)(dt, 0.1f);

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
                
                if (event.type == SDL_MOUSEMOTION) {
                    instance_->send_pointer_event((float)event.motion.x, (float)event.motion.y, (event.motion.state & SDL_BUTTON_LMASK) != 0);
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    instance_->send_pointer_event((float)event.button.x, (float)event.button.y, true);
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    instance_->send_pointer_event((float)event.button.x, (float)event.button.y, false);
                }
            }

            instance_->advance(dt);

            flex_renderer_->begin_frame(1000, 700, 1.0f);
            flex_renderer_->clear(instance_->scene()->background());
            instance_->render(*flex_renderer_);
            flex_renderer_->end_frame();

            SDL_UpdateTexture(texture_, nullptr, buffer_.data(), 1000 * sizeof(uint32_t));
            SDL_RenderClear(sdl_renderer_);
            SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(sdl_renderer_);

            SDL_Delay(1);
        }
    }

    ~ChartExample() {
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();
        if (canvas_) delete canvas_;
        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;
    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
};

int main(int argc, char* argv[]) {
    const char* chart_file = "sample_chart.chart";
    if (argc > 1) chart_file = argv[1];

    ChartExample example;
    if (example.init(chart_file)) {
        example.run();
    }
    return 0;
}
