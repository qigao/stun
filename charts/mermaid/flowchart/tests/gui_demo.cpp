#include <iostream>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdint>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/render/engines/thorvg.h"
#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"

// 外部声明注册函数
namespace flex {
    void register_flowchart_component();
}

class FlowchartGuiDemo {
public:
    static constexpr int WIDTH = 1024;
    static constexpr int HEIGHT = 768;

    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;

        window_ = SDL_CreateWindow("Mermaid C++ Flowchart (OGDF + FlexUI)", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
        sdl_renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

        tvg::Initializer::init(0);
        canvas_ = tvg::SwCanvas::gen();
        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        flex::render::engines::thorvg::init();
        
        // 加载字体，确保文字可见
        if (!flex::render::engines::thorvg::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::render::engines::thorvg::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        flex::register_flowchart_component();

        // 创建一个 flowchart 实例
        flex::Props props;
        props["code"] = "flowchart TD\n  Start-->|Init|B{Is it working?}\n  B-->|Yes|C[Success!]\n  B-->|No|D[Debug...]\n  D-->B";
        props["width"] = 900.0f;
        props["height"] = 600.0f;

        node_ = flex::create_component_instance("flowchart", props);
        node_->set_position(50, 50);

        flex_renderer_ = flex::render::engines::thorvg::create_renderer(canvas_);
        return true;
    }

    void run() {
        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
            }

            // 渲染
            canvas_->remove();
            flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
            flex_renderer_->clear(flex::Color(255, 255, 255, 255)); // 白色背景
            
            if (node_) {
                node_->render(*flex_renderer_);
            }

            flex_renderer_->end_frame();
            canvas_->draw(true);
            canvas_->sync();

            SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));
            SDL_RenderClear(sdl_renderer_);
            SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(sdl_renderer_);
            SDL_Delay(16);
        }
    }

    ~FlowchartGuiDemo() {
        flex::render::engines::thorvg::shutdown();
        tvg::Initializer::term();
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(sdl_renderer_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<std::uint32_t> buffer_;
    std::shared_ptr<flex::Node> node_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
};

int main(int argc, char* argv[]) {
    FlowchartGuiDemo demo;
    if (demo.init()) {
        demo.run();
    }
    return 0;
}
