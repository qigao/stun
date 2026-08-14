/*
 * Shopping Cart Demo - MVC Refactor
 * E-commerce shopping cart with dynamic price calculation and interactions
 * Refactored using Model-View-Controller pattern.
 */

#include <SDL2/SDL.h>
#include <flex.h>
#include "flex/render/engines/thorvg.h"
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <thorvg.h>
#include <vector>

// ============================================================================
// Model - Data and Business Logic
// ============================================================================

struct Product {
  std::string name;
  float price;
  int quantity;
  int max_stock;

  float subtotal() const { return price * quantity; }
};

class ShoppingCartModel {
public:
  static constexpr float TAX_RATE = 0.10f;

  ShoppingCartModel() {
    products_ = {{"Gaming Laptop", 999.0f, 1, 5},
                 {"Wireless Mouse", 49.0f, 2, 10},
                 {"Mechanical Keyboard", 129.0f, 1, 8}};
  }

  void increment(int index) {
    if (index >= 0 && index < (int)products_.size()) {
      if (products_[index].quantity < products_[index].max_stock) {
        products_[index].quantity++;
      }
    }
  }

  void decrement(int index) {
    if (index >= 0 && index < (int)products_.size()) {
      if (products_[index].quantity > 0) {
        products_[index].quantity--;
      }
    }
  }

  void clear() {
    for (auto &p : products_)
      p.quantity = 0;
  }

  float get_subtotal() const {
    float s = 0;
    for (const auto &p : products_)
      s += p.subtotal();
    return s;
  }

  float get_tax() const { return get_subtotal() * TAX_RATE; }
  float get_total() const { return get_subtotal() + get_tax(); }

  int get_total_items() const {
    int count = 0;
    for (const auto &p : products_)
      count += p.quantity;
    return count;
  }

  const std::vector<Product> &products() const { return products_; }
  bool is_empty() const { return get_total_items() == 0; }

private:
  std::vector<Product> products_;
};

// ============================================================================
// View - UI Representation and Updates
// ============================================================================

class ShoppingCartView {
public:
  struct ItemNodes {
    flex::Node *group;
    flex::Node *name_text;
    flex::Node *price_text;
    flex::Node *qty_text;
    flex::Node *subtotal_text;
    flex::Node *minus_btn;
    flex::Node *plus_btn;
  };

  ShoppingCartView(flex::Instance::SharedPtr instance) : instance_(instance) {
    auto *scene = instance_->scene();
    // Initialize child nodes from scene
    item_count_text_ = scene->find("itemCount");
    empty_overlay_ = scene->find("emptyOverlay");

    // Find repeated item rows until the scene stops exposing them.
    for (int i = 0;; ++i) {
      std::string id = "item" + std::to_string(i);
      auto *item_row = scene->find(id);
      if (!item_row) {
        break;
      }

      items_.push_back({item_row,
                        item_row->find("itemName"),
                        item_row->find("itemPrice"),
                        item_row->find("qty"),
                        item_row->find("subtotal"),
                        item_row->find("minusBtn"),
                        item_row->find("plusBtn")});
    }

    if (items_.empty()) {
      std::cerr << "Warning: Could not find any repeated shopping cart rows\n";
    }

    // Summary nodes
    subtotal_text_ = scene->find("subtotalValue");
    tax_text_ = scene->find("taxValue");
    total_text_ = scene->find("totalValue");

    // Action nodes
    checkout_btn_ = scene->find("checkoutBtn");
    clear_btn_ = scene->find("clearBtn");
  }

  void update(const ShoppingCartModel &model) {
    // Update item count header
    set_text(item_count_text_, std::to_string(model.get_total_items()));

    // Update products
    const auto &products = model.products();
    for (size_t i = 0; i < items_.size(); ++i) {
      if (i >= products.size()) {
        items_[i].group->set_visible(false);
        continue;
      }

      const auto &product = products[i];
      items_[i].group->set_visible(true);
      items_[i].group->set_opacity(product.quantity > 0 ? 1.0f : 0.45f);
      set_text(items_[i].name_text, product.name);
      set_text(items_[i].price_text, format_price(product.price));
      set_text(items_[i].qty_text, std::to_string(product.quantity));
      set_text(items_[i].subtotal_text, format_price(product.subtotal()));
      if (items_[i].minus_btn) {
        items_[i].minus_btn->set_opacity(product.quantity > 0 ? 1.0f : 0.35f);
      }
      if (items_[i].plus_btn) {
        items_[i].plus_btn->set_opacity(product.quantity < product.max_stock ? 1.0f : 0.35f);
      }
    }

    // Update summary
    set_text(subtotal_text_, format_price(model.get_subtotal()));
    set_text(tax_text_, format_price(model.get_tax()));
    set_text(total_text_, format_price(model.get_total()));
  }

  const std::vector<ItemNodes> &item_nodes() const { return items_; }
  flex::Node *checkout_btn() const { return checkout_btn_; }
  flex::Node *clear_btn() const { return clear_btn_; }
  flex::Instance::SharedPtr instance() const { return instance_; }

private:
  void set_text(flex::Node *node, const std::string &content) {
    if (auto *text = dynamic_cast<flex::Text *>(node)) {
      text->set_content(content);
    }
  }

  std::string format_price(float price) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << price;
    return oss.str();
  }

  flex::Instance::SharedPtr instance_;
  flex::Node *item_count_text_ = nullptr;
  flex::Node *empty_overlay_ = nullptr;
  std::vector<ItemNodes> items_;
  flex::Node *subtotal_text_ = nullptr;
  flex::Node *tax_text_ = nullptr;
  flex::Node *total_text_ = nullptr;
  flex::Node *checkout_btn_ = nullptr;
  flex::Node *clear_btn_ = nullptr;
};

// ============================================================================
// Controller - Interaction Handling
// ============================================================================

class ShoppingCartController {
public:
  ShoppingCartController(ShoppingCartModel &model, ShoppingCartView &view)
      : model_(model), view_(view), instance_(view.instance()) {

    sync_state_inputs();

    const auto &item_nodes = view_.item_nodes();
    for (size_t i = 0; i < item_nodes.size(); ++i) {
      if (auto *btn = item_nodes[i].minus_btn) {
        btn->on_click([this, i]() {
          model_.decrement((int)i);
          view_.update(model_);
          sync_state_inputs();
          view_.item_nodes()[i].minus_btn->set_scale(1.1f, 1.1f);
          std::cout << "Decremented " << model_.products()[i].name << "\n";
        });
      }
      if (auto *btn = item_nodes[i].plus_btn) {
        btn->on_click([this, i]() {
          model_.increment((int)i);
          view_.update(model_);
          sync_state_inputs();
          view_.item_nodes()[i].plus_btn->set_scale(1.1f, 1.1f);
          std::cout << "Incremented " << model_.products()[i].name << "\n";
        });
      }
    }

    if (auto *btn = view_.checkout_btn()) {
      btn->on_click([this]() {
        bool success = perform_checkout();
        view_.update(model_);
        sync_state_inputs();
        trigger_pulse(success ? "checkout" : "actionError");
        if (success) {
          trigger_pulse("actionSuccess");
        }
        view_.checkout_btn()->set_scale(1.05f, 1.05f);
      });
    }

    if (auto *btn = view_.clear_btn()) {
      btn->on_click([this]() {
        model_.clear();
        view_.update(model_);
        sync_state_inputs();
        trigger_pulse("clear");
        view_.clear_btn()->set_scale(1.05f, 1.05f);
        std::cout << "Cart cleared\n";
      });
    }
  }

  void update(float dt) {
    update_pulse(checkout_pulse_, "checkout", dt);
    update_pulse(clear_pulse_, "clear", dt);
    update_pulse(success_pulse_, "actionSuccess", dt);
    update_pulse(error_pulse_, "actionError", dt);
    sync_state_inputs();

    // Simple cooldown for button scales
    for (const auto &item : view_.item_nodes()) {
      reset_scale(item.minus_btn);
      reset_scale(item.plus_btn);
    }
    reset_scale(view_.checkout_btn());
    reset_scale(view_.clear_btn());
  }

private:
  void reset_scale(flex::Node *node) {
    if (node && node->scale_x() > 1.0f) {
      node->set_scale(1.0f, 1.0f);
    }
  }

  void sync_state_inputs() {
    if (!instance_) {
      return;
    }
    instance_->set_input("itemCount", (float)model_.get_total_items());
  }

  void trigger_pulse(const char *name, float duration = 0.12f) {
    if (!instance_) {
      return;
    }

    instance_->set_input(name, 1.0f);
    if (std::strcmp(name, "checkout") == 0) {
      checkout_pulse_ = duration;
    } else if (std::strcmp(name, "clear") == 0) {
      clear_pulse_ = duration;
    } else if (std::strcmp(name, "actionSuccess") == 0) {
      success_pulse_ = duration;
    } else if (std::strcmp(name, "actionError") == 0) {
      error_pulse_ = duration;
    }
  }

  void update_pulse(float &remaining, const char *name, float dt) {
    if (!instance_ || remaining <= 0.0f) {
      return;
    }

    remaining -= dt;
    if (remaining <= 0.0f) {
      remaining = 0.0f;
      instance_->set_input(name, 0.0f);
    }
  }

  bool perform_checkout() {
    if (model_.is_empty()) {
      std::cout << "Cart is empty! Add some items first.\n";
      return false;
    }

    std::cout << "\n===========================================\n";
    std::cout << "CHECKOUT SUMMARY\n";
    std::cout << "===========================================\n";
    for (const auto &p : model_.products()) {
      if (p.quantity > 0) {
        std::cout << std::left << std::setw(20) << p.name << " x" << p.quantity << " = $"
                  << std::fixed << std::setprecision(2) << p.subtotal() << "\n";
      }
    }
    std::cout << "-------------------------------------------\n";
    std::cout << "Subtotal: $" << std::fixed << std::setprecision(2) << model_.get_subtotal()
              << "\n";
    std::cout << "Tax:      $" << std::fixed << std::setprecision(2) << model_.get_tax() << "\n";
    std::cout << "TOTAL:    $" << std::fixed << std::setprecision(2) << model_.get_total() << "\n";
    std::cout << "===========================================\n";
    std::cout << "Thank you for your purchase!\n\n";

    model_.clear();
    return true;
  }

  ShoppingCartModel &model_;
  ShoppingCartView &view_;
  flex::Instance::SharedPtr instance_;
  float checkout_pulse_ = 0.0f;
  float clear_pulse_ = 0.0f;
  float success_pulse_ = 0.0f;
  float error_pulse_ = 0.0f;
};

// ============================================================================
// ShoppingCartDemo - Application Shell
// ============================================================================

class ShoppingCartDemo {
public:
  bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
      return false;
    }

    window_ = SDL_CreateWindow("Shopping Cart - Flex Engine MVC Demo", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
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

    flex::render::engines::thorvg::init();

    if (!flex::render::engines::thorvg::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
      flex::render::engines::thorvg::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
    }

    auto definition = flex::Definition::load_file("shopping_cart.flex");
    if (definition->has_error()) {
      std::cerr << "Parse error: " << definition->error_message() << "\n";
      return false;
    }

    instance_ = flex::Instance::create(definition);
    if (!instance_->scene())
      return false;

    flex_renderer_ = flex::render::engines::thorvg::create_renderer(canvas_);

    // Initialize MVC
    model_ = std::make_unique<ShoppingCartModel>();
    view_ = std::make_unique<ShoppingCartView>(instance_);
    controller_ = std::make_unique<ShoppingCartController>(*model_, *view_);

    // Sync initial state
    view_->update(*model_);

    std::cout << "Shopping Cart MVC Demo initialized!\n";
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

      // Update MVC and Engine
      controller_->update(dt);
      instance_->advance(dt);

      render();
      SDL_Delay(16);
    }
  }

  ~ShoppingCartDemo() {
    controller_.reset();
    view_.reset();
    model_.reset();
    flex_renderer_.reset();
    instance_.reset();
    flex::render::engines::thorvg::shutdown();

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
  static constexpr int WIDTH = 500;
  static constexpr int HEIGHT = 700;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *sdl_renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;

  tvg::SwCanvas *canvas_ = nullptr;
  std::vector<uint32_t> buffer_;

  flex::Instance::SharedPtr instance_;
  std::unique_ptr<flex::Renderer> flex_renderer_;

  std::unique_ptr<ShoppingCartModel> model_;
  std::unique_ptr<ShoppingCartView> view_;
  std::unique_ptr<ShoppingCartController> controller_;

  bool running_ = false;

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
        }
        break;

      case SDL_MOUSEBUTTONDOWN:
        instance_->send_pointer_event((float)event.button.x, (float)event.button.y, true);
        break;
      case SDL_MOUSEBUTTONUP:
        instance_->send_pointer_event((float)event.button.x, (float)event.button.y, false);
        break;
      case SDL_MOUSEMOTION:
        instance_->send_pointer_event((float)event.motion.x, (float)event.motion.y,
                                      (event.motion.state & SDL_BUTTON_LMASK) != 0);
        break;
      }
    }
  }

  void render() {
    canvas_->remove();

    flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
    flex_renderer_->clear(instance_->scene()->background());
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
  ShoppingCartDemo demo;
  if (!demo.init()) {
    return 1;
  }

  demo.run();
  return 0;
}
