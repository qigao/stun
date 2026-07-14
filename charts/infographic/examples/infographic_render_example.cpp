#include <SDL2/SDL.h>
#include <flex.h>
#include <backends/thorvg/init.h>
#include <flexinfographic.h>
#include <infographic_component.h>
#include <flex/runtime/instance.h>
#include <iostream>
#include <thorvg.h>


using namespace flex;
using namespace flex::modules::infographic;

class InfographicExample {
public:
  bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
      return false;

    window_ = SDL_CreateWindow("Infographic Render Example", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    sdl_renderer_ =
        SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    flex::init();
    canvas_ = tvg::SwCanvas::gen();
    buffer_.resize(WIDTH * HEIGHT);
    canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);
    flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf");

    // Create infographic programmatically
    auto info = create_infographic(TemplateType::ChartPiePlainText);
    info->set_title("Market Share Analysis");
    
    // Add items (Values estimated from image)
    info->add_item(DataItem::create_with_value("Product A", 40.0));
    info->add_item(DataItem::create_with_value("Product B", 30.0));
    info->add_item(DataItem::create_with_value("Product C", 20.0));
    info->add_item(DataItem::create_with_value("Others", 10.0));

    instance_ = Instance::create(WIDTH, HEIGHT);
    auto *scene = instance_->scene();
    scene->set_background(Color(0.95f, 0.95f, 0.97f));

    auto *node = InfographicComponent::build(*info, *instance_);
    if (node) {
      node->set_position(50, 50);
      scene->root()->add_child(node);
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
        if (event.type == SDL_QUIT)
          running = false;
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

  ~InfographicExample() {
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

  SDL_Window *window_ = nullptr;
  SDL_Renderer *sdl_renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;
  tvg::SwCanvas *canvas_ = nullptr;
  std::vector<uint32_t> buffer_;
  Instance::Ptr instance_;
  std::unique_ptr<Renderer> flex_renderer_;
};

int main(int argc, char *argv[]) {
  InfographicExample example;
  if (example.init()) {
    example.run();
  }
  return 0;
}
