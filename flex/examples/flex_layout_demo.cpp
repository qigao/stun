/*
 * Flex Layout Demo
 * Demonstrates automatic layout using Flexbox
 */

#include <SDL2/SDL.h>
#include <flex.h>
#include <iostream>
#include <thorvg.h>

class FlexLayoutDemo {
public:
  bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
      return false;
    }

    window_ = SDL_CreateWindow("Flex Layout Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!window_)
      return false;

    sdl_renderer_ =
        SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer_)
      return false;

    texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (!texture_)
      return false;

    if (tvg::Initializer::init(0) != tvg::Result::Success)
      return false;

    canvas_ = tvg::SwCanvas::gen();
    if (!canvas_)
      return false;

    buffer_.resize(WIDTH * HEIGHT);
    canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

    flex::init();

    if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
      flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
    }

    std::cout << "Loading Flex Layout Demo...\n";
    auto definition = flex::Definition::load_file("flex_layout.flex");

    if (definition->has_error()) {
      std::cerr << "Parse error: " << definition->error_message() << "\n";
      return false;
    }

    instance_ = flex::Instance::create(definition);
    if (!instance_->artboard())
      return false;

    flex_renderer_ = flex::create_thorvg_renderer(canvas_);

    setup_layouts();

    std::cout << "Flex Layout Demo initialized!\n";
    std::cout << "Controls:\n";
    std::cout << "  R - Re-apply layouts\n";
    std::cout << "  ESC - Quit\n\n";

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
      SDL_Delay(16); // Limit to ~60 FPS
    }
  }

  ~FlexLayoutDemo() {
    flex_renderer_.reset();
    instance_.reset();
    flex::shutdown();

    if (canvas_)
      delete canvas_;
    tvg::Initializer::term();

    if (texture_)
      SDL_DestroyTexture(texture_);
    if (sdl_renderer_)
      SDL_DestroyRenderer(sdl_renderer_);
    if (window_)
      SDL_DestroyWindow(window_);
    SDL_Quit();
  }

private:
  static constexpr int WIDTH = 1000;
  static constexpr int HEIGHT = 700;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *sdl_renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;

  tvg::SwCanvas *canvas_ = nullptr;
  std::vector<uint32_t> buffer_;

  flex::Instance::Ptr instance_;
  std::unique_ptr<flex::Renderer> flex_renderer_;

  bool running_ = false;

  void setup_layouts() {
    auto *artboard = instance_->artboard();

    // Scene 1: Horizontal Toolbar (Flex Row)
    if (auto *toolbar = dynamic_cast<flex::Group *>(artboard->find("scene1")->find("toolbar"))) {
      toolbar->set_layout(flex::LayoutMode::Flex);
      toolbar->set_flex_direction(flex::FlexDirection::Row);
      toolbar->set_gap(10.0f);
      toolbar->set_clip(true);
      toolbar->set_clip_size(420, 60);
      toolbar->perform_layout();

      std::cout << "✅ Scene 1: Horizontal toolbar (row, gap:10)\n";
    }

    // Scene 2: Vertical Menu (Flex Column)
    if (auto *menu = dynamic_cast<flex::Group *>(artboard->find("scene2")->find("menu"))) {
      menu->set_layout(flex::LayoutMode::Flex);
      menu->set_flex_direction(flex::FlexDirection::Column);
      menu->set_gap(5.0f);
      menu->set_clip(true);
      menu->set_clip_size(220, 100);
      menu->perform_layout();

      std::cout << "✅ Scene 2: Vertical menu (column, gap:5)\n";
    }

    // Scene 3: Center Alignment
    if (auto *centerContainer =
            dynamic_cast<flex::Group *>(artboard->find("scene3")->find("centerContainer"))) {
      centerContainer->set_layout(flex::LayoutMode::Flex);
      centerContainer->set_flex_direction(flex::FlexDirection::Row);
      centerContainer->set_justify_content(flex::JustifyContent::Center);
      centerContainer->set_align_items(flex::AlignItems::Center);
      centerContainer->set_clip(true);
      centerContainer->set_clip_size(400, 100);
      centerContainer->perform_layout();

      std::cout << "✅ Scene 3: Centered layout (justify:center, align:center)\n";
    }

    // Scene 4: Space Between
    if (auto *spaceBetween =
            dynamic_cast<flex::Group *>(artboard->find("scene4")->find("spaceBetweenContainer"))) {
      spaceBetween->set_layout(flex::LayoutMode::Flex);
      spaceBetween->set_flex_direction(flex::FlexDirection::Row);
      spaceBetween->set_justify_content(flex::JustifyContent::SpaceBetween);
      spaceBetween->set_clip(true);
      spaceBetween->set_clip_size(400, 80);
      spaceBetween->perform_layout();

      std::cout << "✅ Scene 4: Space between (justify:space-between)\n";
    }

    // Scene 5: Nested Layout (Column > Row)
    if (auto *outerColumn =
            dynamic_cast<flex::Group *>(artboard->find("scene5")->find("outerColumn"))) {
      // Outer column
      outerColumn->set_layout(flex::LayoutMode::Flex);
      outerColumn->set_flex_direction(flex::FlexDirection::Column);
      outerColumn->set_gap(10.0f);
      outerColumn->set_clip(true);
      outerColumn->set_clip_size(180, 100);

      // Inner rows
      if (auto *row1 = dynamic_cast<flex::Group *>(outerColumn->find("row1"))) {
        row1->set_layout(flex::LayoutMode::Flex);
        row1->set_flex_direction(flex::FlexDirection::Row);
        row1->set_gap(5.0f);
        row1->perform_layout();
      }

      if (auto *row2 = dynamic_cast<flex::Group *>(outerColumn->find("row2"))) {
        row2->set_layout(flex::LayoutMode::Flex);
        row2->set_flex_direction(flex::FlexDirection::Row);
        row2->set_gap(5.0f);
        row2->perform_layout();
      }

      outerColumn->perform_layout();

      std::cout << "✅ Scene 5: Nested layout (column containing rows)\n";
    }

    // Scene 6: Padding & Gap
    if (auto *paddedContainer =
            dynamic_cast<flex::Group *>(artboard->find("scene6")->find("paddedContainer"))) {
      paddedContainer->set_layout(flex::LayoutMode::Flex);
      paddedContainer->set_flex_direction(flex::FlexDirection::Row);
      paddedContainer->set_gap(15.0f);
      paddedContainer->set_padding(20.0f); // All sides
      paddedContainer->set_clip(true);
      paddedContainer->set_clip_size(300, 80);
      paddedContainer->perform_layout();

      std::cout << "✅ Scene 6: Padding & gap (padding:20, gap:15)\n";
    }

    std::cout << "\n✨ All layouts applied!\n\n";
  }

  void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        running_ = false;
        break;

      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running_ = false;
        } else if (event.key.keysym.sym == SDLK_r) {
          std::cout << "🔄 Re-applying layouts...\n";
          setup_layouts();
        }
        break;
      }
    }
  }

  void update(float dt) { instance_->advance(dt); }

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

int main(int argc, char *argv[]) {
  std::cout << "===========================================\n";
  std::cout << "Flex Layout Demo\n";
  std::cout << "===========================================\n\n";

  FlexLayoutDemo demo;
  if (!demo.init()) {
    return 1;
  }

  demo.run();
  return 0;
}
