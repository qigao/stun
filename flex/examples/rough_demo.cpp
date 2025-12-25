/*
 * Rough (Hand-drawn) Style Demo
 *
 * Demonstrates hand-drawn style rendering for all shapes
 * Strictly follows nanovg_rough.c implementation
 */

#include <iostream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"
#include <algorithm>
class RoughDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Rough Style Demo - All Shapes",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH, HEIGHT,
            SDL_WINDOW_SHOWN
        );
        if (!window_) {
            std::cerr << "Window creation failed\n";
            return false;
        }

        sdl_renderer_ = SDL_CreateRenderer(window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdl_renderer_) {
            std::cerr << "SDL renderer creation failed\n";
            return false;
        }

        texture_ = SDL_CreateTexture(sdl_renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            WIDTH, HEIGHT);
        if (!texture_) {
            std::cerr << "Texture creation failed\n";
            return false;
        }

        if (tvg::Initializer::init(0) != tvg::Result::Success) {
            std::cerr << "ThorVG init failed\n";
            return false;
        }

        canvas_.reset(tvg::SwCanvas::gen());
        if (!canvas_) {
            std::cerr << "ThorVG canvas creation failed\n";
            return false;
        }

        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        flex::init();

        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        instance_ = flex::Instance::create(WIDTH, HEIGHT);
        auto* artboard = instance_->artboard();

        // Initial style
        current_rough_ = flex::RoughOptions::sketch();
        current_rough_.seed = 42;

        // Row 1: Circle, Ellipse, Rectangle
        // Circle
        auto circle = flex::Shape::create();
        circle->set_circle(40);
        circle->set_fill(flex::Color::Transparent);
        circle->set_stroke(flex::Color(0.2f, 0.4f, 0.8f, 1.0f), 2.5f);
        circle->set_rough(current_rough_);
        circle->set_position(120, 150);
        artboard->add_child(circle);
        shapes_.push_back(circle);

        auto label1 = flex::Text::create();
        label1->set_content("Circle");
        label1->set_font_size(14);
        label1->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label1->set_position(95, 210);
        artboard->add_child(label1);

        // Ellipse
        auto ellipse = flex::Shape::create();
        ellipse->set_ellipse(50, 35);
        ellipse->set_fill(flex::Color(1.0f, 0.9f, 0.7f, 1.0f));
        ellipse->set_stroke(flex::Color(0.8f, 0.6f, 0.2f, 1.0f), 2.5f);
        ellipse->set_rough(current_rough_);
        ellipse->set_position(280, 150);
        artboard->add_child(ellipse);
        shapes_.push_back(ellipse);

        auto label2 = flex::Text::create();
        label2->set_content("Ellipse");
        label2->set_font_size(14);
        label2->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label2->set_position(252, 210);
        artboard->add_child(label2);

        // Rectangle
        auto rect = flex::Shape::create();
        rect->set_rect(90, 65, 8);
        rect->set_fill(flex::Color(0.9f, 0.7f, 0.7f, 1.0f));
        rect->set_stroke(flex::Color(0.7f, 0.2f, 0.2f, 1.0f), 2.5f);
        rect->set_rough(current_rough_);
        rect->set_position(400, 115);
        artboard->add_child(rect);
        shapes_.push_back(rect);

        auto label3 = flex::Text::create();
        label3->set_content("Rectangle");
        label3->set_font_size(14);
        label3->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label3->set_position(410, 210);
        artboard->add_child(label3);

        // Row 2: Triangle, Pentagon, Hexagon
        // Triangle
        auto triangle = flex::Shape::create();
        triangle->set_polygon(3, 45);
        triangle->set_fill(flex::Color(0.7f, 0.9f, 0.7f, 1.0f));
        triangle->set_stroke(flex::Color(0.2f, 0.6f, 0.2f, 1.0f), 2.5f);
        triangle->set_rough(current_rough_);
        triangle->set_position(120, 320);
        artboard->add_child(triangle);
        shapes_.push_back(triangle);

        auto label4 = flex::Text::create();
        label4->set_content("Triangle");
        label4->set_font_size(14);
        label4->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label4->set_position(85, 380);
        artboard->add_child(label4);

        // Pentagon
        auto pentagon = flex::Shape::create();
        pentagon->set_polygon(5, 40);
        pentagon->set_fill(flex::Color(0.9f, 0.7f, 0.9f, 1.0f));
        pentagon->set_stroke(flex::Color(0.6f, 0.2f, 0.6f, 1.0f), 2.5f);
        pentagon->set_rough(current_rough_);
        pentagon->set_position(280, 320);
        artboard->add_child(pentagon);
        shapes_.push_back(pentagon);

        auto label5 = flex::Text::create();
        label5->set_content("Pentagon");
        label5->set_font_size(14);
        label5->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label5->set_position(245, 380);
        artboard->add_child(label5);

        // Hexagon
        auto hexagon = flex::Shape::create();
        hexagon->set_polygon(6, 40);
        hexagon->set_fill(flex::Color(0.7f, 0.9f, 0.9f, 1.0f));
        hexagon->set_stroke(flex::Color(0.2f, 0.6f, 0.6f, 1.0f), 2.5f);
        hexagon->set_rough(current_rough_);
        hexagon->set_position(445, 320);
        artboard->add_child(hexagon);
        shapes_.push_back(hexagon);

        auto label6 = flex::Text::create();
        label6->set_content("Hexagon");
        label6->set_font_size(14);
        label6->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label6->set_position(410, 380);
        artboard->add_child(label6);

        // Row 3: Star, Ring, Line
        // Star
        auto star = flex::Shape::create();
        star->set_star(5, 50, 20);
        star->set_fill(flex::Color(1.0f, 0.9f, 0.6f, 1.0f));
        star->set_stroke(flex::Color(0.8f, 0.7f, 0.2f, 1.0f), 2.5f);
        star->set_rough(current_rough_);
        star->set_position(120, 490);
        artboard->add_child(star);
        shapes_.push_back(star);

        auto label7 = flex::Text::create();
        label7->set_content("Star");
        label7->set_font_size(14);
        label7->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label7->set_position(100, 550);
        artboard->add_child(label7);

        // Ring
        auto ring = flex::Shape::create();
        ring->set_ring(45, 25);
        ring->set_fill(flex::Color(0.8f, 0.8f, 0.9f, 1.0f));
        ring->set_stroke(flex::Color(0.3f, 0.3f, 0.6f, 1.0f), 2.5f);
        ring->set_rough(current_rough_);
        ring->set_position(280, 490);
        artboard->add_child(ring);
        shapes_.push_back(ring);

        auto label8 = flex::Text::create();
        label8->set_content("Ring");
        label8->set_font_size(14);
        label8->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label8->set_position(260, 550);
        artboard->add_child(label8);

        // Line
        auto line = flex::Shape::create();
        line->set_line(80, 60);
        line->set_stroke(flex::Color(0.4f, 0.4f, 0.4f, 1.0f), 3.0f);
        line->set_rough(current_rough_);
        line->set_position(405, 460);
        artboard->add_child(line);
        shapes_.push_back(line);

        auto label9 = flex::Text::create();
        label9->set_content("Line");
        label9->set_font_size(14);
        label9->set_color(flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
        label9->set_position(430, 550);
        artboard->add_child(label9);

        // Title
        auto title = flex::Text::create();
        title->set_content("Hand-drawn Style - All Shapes");
        title->set_font_size(24);
        title->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
        title->set_position(150, 40);
        artboard->add_child(title);

        flex_renderer_ = flex::create_thorvg_renderer(canvas_.get());

        std::cout << "All shapes rendered with current style\n";
        std::cout << "Press 1: Sketch, 2: Rough, 3: Very Rough, 4: Cross-Hatch\n";
        std::cout << "Press ESC to quit\n";

        return true;
    }

    void run() {
        running_ = true;

        while (running_) {
            handle_events();
            render();
            SDL_Delay(16);
        }
    }

    ~RoughDemo() {
        flex_renderer_.reset();
        instance_.reset();
        flex::shutdown();

        if (canvas_) canvas_.reset();
        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running_ = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running_ = false;
                } else if (event.key.keysym.sym == SDLK_1) {
                    update_style(flex::RoughOptions::sketch());
                } else if (event.key.keysym.sym == SDLK_2) {
                    update_style(flex::RoughOptions::rough());
                } else if (event.key.keysym.sym == SDLK_3) {
                    update_style(flex::RoughOptions::very_rough());
                } else if (event.key.keysym.sym == SDLK_4) {
                    auto cross = flex::RoughOptions::very_rough();
                    cross.fill_style = flex::RoughFillStyle::CrossHatch;
                    update_style(cross);
                }
            }
        }
    }

    void update_style(flex::RoughOptions opts) {
        current_rough_ = opts;
        current_rough_.seed = 42;
        for (auto& shape : shapes_) {
            shape->set_rough(current_rough_);
        }
        std::cout << "Switched to new style\n";
    }

    void render() {
        SDL_SetRenderDrawColor(sdl_renderer_, 242, 242, 242, 255);
        SDL_RenderClear(sdl_renderer_);

        flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
        instance_->render(*flex_renderer_);
        flex_renderer_->end_frame();

        void* pixels;
        int pitch;
        SDL_LockTexture(texture_, nullptr, &pixels, &pitch);
        memcpy(pixels, buffer_.data(), WIDTH * HEIGHT * 4);
        SDL_UnlockTexture(texture_);

        SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer_);
    }

    static const int WIDTH = 600;
    static const int HEIGHT = 620;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
    std::vector<flex::Shape::Ptr> shapes_;
    flex::RoughOptions current_rough_ = flex::RoughOptions::disabled();

    bool running_ = false;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    RoughDemo demo;
    if (!demo.init()) {
        std::cerr << "Failed to initialize demo\n";
        return 1;
    }

    demo.run();
    return 0;
}
