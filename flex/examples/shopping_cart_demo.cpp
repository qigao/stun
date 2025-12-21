/*
 * Shopping Cart Demo
 * E-commerce shopping cart with dynamic price calculation and interactions
 */

#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>

// Product structure
struct Product {
    std::string name;
    float price;
    int quantity;
    int max_stock;

    float subtotal() const {
        return price * quantity;
    }
};

class ShoppingCartDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Shopping Cart - Flex Engine Demo",
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

        std::cout << "Loading Shopping Cart Demo...\n";
        auto definition = flex::Definition::load_file("shopping_cart.flex");

        if (definition->has_error()) {
            std::cerr << "Parse error: " << definition->error_message() << "\n";
            return false;
        }

        instance_ = flex::Instance::create(definition);
        if (!instance_->artboard()) return false;

        flex_renderer_ = flex::create_thorvg_renderer(canvas_);

        // Initialize products
        products_ = {
            {"Gaming Laptop", 999.0f, 1, 5},
            {"Wireless Mouse", 49.0f, 2, 10},
            {"Mechanical Keyboard", 129.0f, 1, 8}
        };

        // Find UI elements
        auto* artboard = instance_->artboard();

        item_count_text_ = artboard->find("itemCount");
        empty_message_ = artboard->find("emptyMessage");

        // Item 1 elements
        item1_qty_text_ = artboard->find("item1")->find("qty");
        item1_subtotal_text_ = artboard->find("subtotal1");
        item1_minus_btn_ = artboard->find("item1")->find("qtyControls")->find("minusBtn");
        item1_plus_btn_ = artboard->find("item1")->find("qtyControls")->find("plusBtn");

        // Item 2 elements
        item2_qty_text_ = artboard->find("item2")->find("qty");
        item2_subtotal_text_ = artboard->find("subtotal2");
        item2_minus_btn_ = artboard->find("item2")->find("qtyControls")->find("minusBtn");
        item2_plus_btn_ = artboard->find("item2")->find("qtyControls")->find("plusBtn");

        // Item 3 elements
        item3_qty_text_ = artboard->find("item3")->find("qty");
        item3_subtotal_text_ = artboard->find("subtotal3");
        item3_minus_btn_ = artboard->find("item3")->find("qtyControls")->find("minusBtn");
        item3_plus_btn_ = artboard->find("item3")->find("qtyControls")->find("plusBtn");

        // Item groups (for hiding when empty)
        item1_group_ = artboard->find("item1");
        item2_group_ = artboard->find("item2");
        item3_group_ = artboard->find("item3");

        // Summary elements
        subtotal_value_text_ = artboard->find("subtotalValue");
        tax_value_text_ = artboard->find("taxValue");
        total_value_text_ = artboard->find("totalValue");

        // Action buttons
        checkout_btn_ = artboard->find("checkoutBtn");
        clear_btn_ = artboard->find("clearBtn");

        update_cart_display();

        std::cout << "Shopping Cart Demo initialized!\n";
        std::cout << "Controls:\n";
        std::cout << "  Click +/- to adjust quantities\n";
        std::cout << "  Click Checkout to complete purchase\n";
        std::cout << "  Click Clear to empty cart\n";
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
        }
    }

    ~ShoppingCartDemo() {
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
    static constexpr int WIDTH = 500;
    static constexpr int HEIGHT = 700;
    static constexpr float TAX_RATE = 0.10f;  // 10% tax

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;

    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Product data
    std::vector<Product> products_;

    // UI elements
    flex::Node* item_count_text_ = nullptr;
    flex::Node* empty_message_ = nullptr;

    flex::Node* item1_qty_text_ = nullptr;
    flex::Node* item1_subtotal_text_ = nullptr;
    flex::Node* item1_minus_btn_ = nullptr;
    flex::Node* item1_plus_btn_ = nullptr;
    flex::Node* item1_group_ = nullptr;

    flex::Node* item2_qty_text_ = nullptr;
    flex::Node* item2_subtotal_text_ = nullptr;
    flex::Node* item2_minus_btn_ = nullptr;
    flex::Node* item2_plus_btn_ = nullptr;
    flex::Node* item2_group_ = nullptr;

    flex::Node* item3_qty_text_ = nullptr;
    flex::Node* item3_subtotal_text_ = nullptr;
    flex::Node* item3_minus_btn_ = nullptr;
    flex::Node* item3_plus_btn_ = nullptr;
    flex::Node* item3_group_ = nullptr;

    flex::Node* subtotal_value_text_ = nullptr;
    flex::Node* tax_value_text_ = nullptr;
    flex::Node* total_value_text_ = nullptr;

    flex::Node* checkout_btn_ = nullptr;
    flex::Node* clear_btn_ = nullptr;

    bool running_ = false;

    std::string format_price(float price) {
        std::ostringstream oss;
        oss << "$" << std::fixed << std::setprecision(2) << price;
        return oss.str();
    }

    int get_total_items() {
        int total = 0;
        for (const auto& product : products_) {
            total += product.quantity;
        }
        return total;
    }

    float get_subtotal() {
        float total = 0.0f;
        for (const auto& product : products_) {
            total += product.subtotal();
        }
        return total;
    }

    float get_tax() {
        return get_subtotal() * TAX_RATE;
    }

    float get_total() {
        return get_subtotal() + get_tax();
    }

    bool is_cart_empty() {
        return get_total_items() == 0;
    }

    void update_cart_display() {
        // Update item count
        if (item_count_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(item_count_text_)) {
                text->set_content(std::to_string(get_total_items()));
            }
        }

        // Update item 1
        if (item1_qty_text_ && item1_subtotal_text_) {
            if (auto* qty_text = dynamic_cast<flex::Text*>(item1_qty_text_)) {
                qty_text->set_content(std::to_string(products_[0].quantity));
            }
            if (auto* subtotal_text = dynamic_cast<flex::Text*>(item1_subtotal_text_)) {
                subtotal_text->set_content(format_price(products_[0].subtotal()));
            }
        }

        // Update item 2
        if (item2_qty_text_ && item2_subtotal_text_) {
            if (auto* qty_text = dynamic_cast<flex::Text*>(item2_qty_text_)) {
                qty_text->set_content(std::to_string(products_[1].quantity));
            }
            if (auto* subtotal_text = dynamic_cast<flex::Text*>(item2_subtotal_text_)) {
                subtotal_text->set_content(format_price(products_[1].subtotal()));
            }
        }

        // Update item 3
        if (item3_qty_text_ && item3_subtotal_text_) {
            if (auto* qty_text = dynamic_cast<flex::Text*>(item3_qty_text_)) {
                qty_text->set_content(std::to_string(products_[2].quantity));
            }
            if (auto* subtotal_text = dynamic_cast<flex::Text*>(item3_subtotal_text_)) {
                subtotal_text->set_content(format_price(products_[2].subtotal()));
            }
        }

        // Hide items with quantity 0
        if (item1_group_) item1_group_->set_visible(products_[0].quantity > 0);
        if (item2_group_) item2_group_->set_visible(products_[1].quantity > 0);
        if (item3_group_) item3_group_->set_visible(products_[2].quantity > 0);

        // Update summary
        if (subtotal_value_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(subtotal_value_text_)) {
                text->set_content(format_price(get_subtotal()));
            }
        }

        if (tax_value_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(tax_value_text_)) {
                text->set_content(format_price(get_tax()));
            }
        }

        if (total_value_text_) {
            if (auto* text = dynamic_cast<flex::Text*>(total_value_text_)) {
                text->set_content(format_price(get_total()));
            }
        }

        // Show/hide empty message
        if (empty_message_) {
            empty_message_->set_opacity(is_cart_empty() ? 1.0f : 0.0f);
        }

        std::cout << "Cart updated - Items: " << get_total_items()
                  << ", Total: " << format_price(get_total()) << "\n";
    }

    void increment_quantity(int product_index) {
        if (product_index < 0 || product_index >= (int)products_.size()) return;

        auto& product = products_[product_index];
        if (product.quantity < product.max_stock) {
            product.quantity++;
            update_cart_display();

            // Visual feedback
            if (product_index == 0 && item1_plus_btn_) item1_plus_btn_->set_scale(1.1f, 1.1f);
            if (product_index == 1 && item2_plus_btn_) item2_plus_btn_->set_scale(1.1f, 1.1f);
            if (product_index == 2 && item3_plus_btn_) item3_plus_btn_->set_scale(1.1f, 1.1f);
        } else {
            std::cout << "Max stock reached for " << product.name << "\n";
        }
    }

    void decrement_quantity(int product_index) {
        if (product_index < 0 || product_index >= (int)products_.size()) return;

        auto& product = products_[product_index];
        if (product.quantity > 0) {
            product.quantity--;
            update_cart_display();

            // Visual feedback
            if (product_index == 0 && item1_minus_btn_) item1_minus_btn_->set_scale(1.1f, 1.1f);
            if (product_index == 1 && item2_minus_btn_) item2_minus_btn_->set_scale(1.1f, 1.1f);
            if (product_index == 2 && item3_minus_btn_) item3_minus_btn_->set_scale(1.1f, 1.1f);
        }
    }

    void checkout() {
        if (is_cart_empty()) {
            std::cout << "Cart is empty! Add some items first.\n";
            return;
        }

        std::cout << "\n===========================================\n";
        std::cout << "CHECKOUT\n";
        std::cout << "===========================================\n";
        for (size_t i = 0; i < products_.size(); i++) {
            if (products_[i].quantity > 0) {
                std::cout << products_[i].name << " x" << products_[i].quantity
                          << " = " << format_price(products_[i].subtotal()) << "\n";
            }
        }
        std::cout << "-------------------------------------------\n";
        std::cout << "Subtotal: " << format_price(get_subtotal()) << "\n";
        std::cout << "Tax:      " << format_price(get_tax()) << "\n";
        std::cout << "TOTAL:    " << format_price(get_total()) << "\n";
        std::cout << "===========================================\n";
        std::cout << "Thank you for your purchase!\n\n";

        // Clear cart after checkout
        clear_cart();
    }

    void clear_cart() {
        for (auto& product : products_) {
            product.quantity = 0;
        }
        update_cart_display();
        std::cout << "Cart cleared\n";
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
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    handle_click(event.button.x, event.button.y);
                    break;
            }
        }
    }

    void handle_click(int x, int y) {
        // Item 1 controls (around y=140)
        if (y >= 130 && y <= 160) {
            if (x >= 320 && x <= 350) {
                decrement_quantity(0);  // Minus button
            } else if (x >= 410 && x <= 440) {
                increment_quantity(0);  // Plus button
            }
        }

        // Item 2 controls (around y=260)
        if (y >= 250 && y <= 280) {
            if (x >= 320 && x <= 350) {
                decrement_quantity(1);
            } else if (x >= 410 && x <= 440) {
                increment_quantity(1);
            }
        }

        // Item 3 controls (around y=380)
        if (y >= 370 && y <= 400) {
            if (x >= 320 && x <= 350) {
                decrement_quantity(2);
            } else if (x >= 410 && x <= 440) {
                increment_quantity(2);
            }
        }

        // Checkout button (centered at 150, 650, width 150, height 45)
        if (x >= 150 && x <= 300 && y >= 650 && y <= 695) {
            checkout();
            if (checkout_btn_) checkout_btn_->set_scale(1.05f, 1.05f);
        }

        // Clear button (centered at 310, 650, width 100, height 45)
        if (x >= 310 && x <= 410 && y >= 650 && y <= 695) {
            clear_cart();
            if (clear_btn_) clear_btn_->set_scale(1.05f, 1.05f);
        }
    }

    void update(float dt) {
        // Reset button scales
        if (item1_minus_btn_ && item1_minus_btn_->scale_x() > 1.0f) {
            item1_minus_btn_->set_scale(1.0f, 1.0f);
        }
        if (item1_plus_btn_ && item1_plus_btn_->scale_x() > 1.0f) {
            item1_plus_btn_->set_scale(1.0f, 1.0f);
        }
        if (item2_minus_btn_ && item2_minus_btn_->scale_x() > 1.0f) {
            item2_minus_btn_->set_scale(1.0f, 1.0f);
        }
        if (item2_plus_btn_ && item2_plus_btn_->scale_x() > 1.0f) {
            item2_plus_btn_->set_scale(1.0f, 1.0f);
        }
        if (item3_minus_btn_ && item3_minus_btn_->scale_x() > 1.0f) {
            item3_minus_btn_->set_scale(1.0f, 1.0f);
        }
        if (item3_plus_btn_ && item3_plus_btn_->scale_x() > 1.0f) {
            item3_plus_btn_->set_scale(1.0f, 1.0f);
        }
        if (checkout_btn_ && checkout_btn_->scale_x() > 1.0f) {
            checkout_btn_->set_scale(1.0f, 1.0f);
        }
        if (clear_btn_ && clear_btn_->scale_x() > 1.0f) {
            clear_btn_->set_scale(1.0f, 1.0f);
        }

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
    std::cout << "Shopping Cart Demo - Flex Engine\n";
    std::cout << "===========================================\n\n";

    ShoppingCartDemo demo;
    if (!demo.init()) {
        return 1;
    }

    demo.run();
    return 0;
}
