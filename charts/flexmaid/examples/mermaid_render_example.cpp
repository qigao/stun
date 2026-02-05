#include <iostream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include <flex/runtime/instance.h>
#include <flex/backends/thorvg/init.h>
#include <flex/modules/flexmaid/flexmaid.h>
#include <flex/modules/flexmaid/mermaid_component.h>

using namespace flex;
using namespace flex::modules::flexmaid;

class MermaidExample {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;

        window_ = SDL_CreateWindow("Mermaid Render Example", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
        sdl_renderer_ = SDL_CreateRenderer(window_, -1, 
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888, 
            SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

        flex::init();
        canvas_ = tvg::SwCanvas::gen();
        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);
        
        flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf");

        // Parse Mermaid diagram
        FlexMaid maid;
        maid.set_theme(Theme::modern());
        
        const char* mermaid_source = R"(
flowchart TD
    A[Start] --> B{Is it working?}
    B -->|Yes| C[Great!]
    B -->|No| D[Debug]
    D --> B
    C --> E[End]
)";

        auto result = maid.parse(mermaid_source);
        if (!result.success) {
            std::cerr << "Parse error: " << result.get_error() << "\n";
            return false;
        }

        // Create Instance and Scene
        instance_ = Instance::create(WIDTH, HEIGHT);
        auto* scene = instance_->scene();
        scene->set_background(Color(0.96f, 0.97f, 0.98f));

        // Build Mermaid diagram as flex node tree
        auto* mermaid_node = MermaidComponent::build(*result.diagram, *instance_);
        if (mermaid_node) {
            mermaid_node->set_position(50, 50);
            scene->root()->add_child(mermaid_node);
        }

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
            dt = std::min(dt, 0.1f);

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) 
                    running = false;
            }

            instance_->advance(dt);

            flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
            flex_renderer_->clear(instance_->scene()->background());
            instance_->render(*flex_renderer_);
            flex_renderer_->end_frame();

            SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));
            SDL_RenderClear(sdl_renderer_);
            SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(sdl_renderer_);

            SDL_Delay(16);
        }
    }

    ~MermaidExample() {
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();
        delete canvas_;
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(sdl_renderer_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;
    
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;
    Instance::Ptr instance_;
    std::unique_ptr<Renderer> flex_renderer_;
};

int main(int argc, char* argv[]) {
    MermaidExample example;
    if (example.init()) {
        example.run();
    }
    return 0;
}
