/*
 * Flex Widgets Gallery Demo
 * Showcases all standard UI controls with Fluent Design theme
 */

#include "flex.h"
#include "widgets/widgets.h"
#include "flex/group.h"
#include "flex/instance.h"
#include <SDL.h>
#include <thorvg.h>
#include <iostream>
#include <vector>
#include <algorithm>

constexpr int WIDTH = 900;
constexpr int HEIGHT = 700;

class WidgetGalleryDemo {
public:
    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Flex Widgets Gallery - Fluent Design",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIDTH, HEIGHT, SDL_WINDOW_SHOWN);

        if (!window_) {
            std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
            return false;
        }

        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
        buffer_.resize(WIDTH * HEIGHT);

        // Initialize ThorVG
        tvg::Initializer::init(0);
        canvas_.reset(tvg::SwCanvas::gen());
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

        // Initialize Flex with widgets
        flex::init();

        // Load font (required for text rendering)
        if (!flex::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
            flex::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
        }

        flex::widgets::register_all_fluent();  // Register all widgets with Fluent theme

        // Create flex instance (handles events)
        instance_ = flex::Instance::create(WIDTH, HEIGHT);

        // Create the demo UI
        create_demo_ui();

        // Create renderer
        flex_renderer_ = flex::create_thorvg_renderer(canvas_.get());

        return true;
    }

    void create_demo_ui() {
        auto* artboard = instance_->artboard();

        // Background
        auto bg = flex::Shape::create();
        bg->set_rect(WIDTH, HEIGHT);
        bg->set_fill(flex::Color(0.95f, 0.95f, 0.95f, 1.0f));  // Light gray background
        artboard->add_child(bg);

        // Title
        auto title = flex::Text::create();
        title->set_content("Flex Widgets Gallery");
        title->set_font_size(28);
        title->set_color(flex::Color(0.1f, 0.1f, 0.1f, 1.0f));
        title->set_position(50, 30);
        artboard->add_child(title);

        // Subtitle
        auto subtitle = flex::Text::create();
        subtitle->set_content("Fluent Design System - Windows 11 Style (Click buttons to test!)");
        subtitle->set_font_size(14);
        subtitle->set_color(flex::Color(0.4f, 0.4f, 0.4f, 1.0f));
        subtitle->set_position(50, 65);
        artboard->add_child(subtitle);

        float y = 110;
        float col1_x = 50;
        float col2_x = 480;

        // ===== Column 1 =====

        // Section: Buttons
        add_section_title("Buttons", col1_x, y);
        y += 35;

        auto btn_accent = flex::create_component_instance("Button", {
            {"text", std::string("Accent Button")},
            {"variant", std::string("accent")},
            {"width", 140.0f}
        });
        btn_accent->set_position(col1_x, y);
        setup_button_events(btn_accent, "Accent");
        artboard->add_child(btn_accent);

        auto btn_secondary = flex::create_component_instance("Button", {
            {"text", std::string("Secondary")},
            {"variant", std::string("secondary")},
            {"width", 120.0f}
        });
        btn_secondary->set_position(col1_x + 160, y);
        setup_button_events(btn_secondary, "Secondary");
        artboard->add_child(btn_secondary);

        auto btn_outline = flex::create_component_instance("Button", {
            {"text", std::string("Outline")},
            {"variant", std::string("outline")},
            {"width", 100.0f}
        });
        btn_outline->set_position(col1_x + 300, y);
        setup_button_events(btn_outline, "Outline");
        artboard->add_child(btn_outline);

        y += 60;

        // Section: Checkboxes & Toggles
        add_section_title("Checkboxes & Toggles", col1_x, y);
        y += 35;

        auto cb1 = flex::create_component_instance("Checkbox", {
            {"text", std::string("Option 1")},
            {"checked", false}
        });
        cb1->set_position(col1_x, y);
        setup_interactive_events(cb1);
        artboard->add_child(cb1);

        auto cb2 = flex::create_component_instance("Checkbox", {
            {"text", std::string("Option 2 (checked)")},
            {"checked", true}
        });
        cb2->set_position(col1_x, y + 30);
        setup_interactive_events(cb2);
        artboard->add_child(cb2);

        auto toggle1 = flex::create_component_instance("Toggle", {
            {"on", false}
        });
        toggle1->set_position(col1_x + 200, y);
        setup_interactive_events(toggle1);
        artboard->add_child(toggle1);

        auto toggle_label1 = flex::Text::create();
        toggle_label1->set_content("Off");
        toggle_label1->set_font_size(14);
        toggle_label1->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
        toggle_label1->set_position(col1_x + 255, y + 3);
        artboard->add_child(toggle_label1);

        auto toggle2 = flex::create_component_instance("Toggle", {
            {"on", true}
        });
        toggle2->set_position(col1_x + 200, y + 30);
        setup_interactive_events(toggle2);
        artboard->add_child(toggle2);

        auto toggle_label2 = flex::Text::create();
        toggle_label2->set_content("On");
        toggle_label2->set_font_size(14);
        toggle_label2->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
        toggle_label2->set_position(col1_x + 255, y + 33);
        artboard->add_child(toggle_label2);

        y += 90;

        // Section: Radio Buttons
        add_section_title("Radio Buttons", col1_x, y);
        y += 35;

        auto radio1 = flex::create_component_instance("RadioButton", {
            {"text", std::string("Option A")},
            {"selected", true},
            {"groupName", std::string("demo")}
        });
        radio1->set_position(col1_x, y);
        setup_interactive_events(radio1);
        artboard->add_child(radio1);

        auto radio2 = flex::create_component_instance("RadioButton", {
            {"text", std::string("Option B")},
            {"selected", false},
            {"groupName", std::string("demo")}
        });
        radio2->set_position(col1_x, y + 30);
        setup_interactive_events(radio2);
        artboard->add_child(radio2);

        auto radio3 = flex::create_component_instance("RadioButton", {
            {"text", std::string("Option C")},
            {"selected", false},
            {"groupName", std::string("demo")}
        });
        radio3->set_position(col1_x, y + 60);
        setup_interactive_events(radio3);
        artboard->add_child(radio3);

        y += 110;

        // Section: Sliders
        add_section_title("Sliders", col1_x, y);
        y += 35;

        auto slider1 = flex::create_component_instance("Slider", {
            {"value", 0.3f},
            {"width", 300.0f}
        });
        slider1->set_position(col1_x, y);
        setup_interactive_events(slider1);
        artboard->add_child(slider1);

        auto slider2 = flex::create_component_instance("Slider", {
            {"value", 0.7f},
            {"width", 300.0f}
        });
        slider2->set_position(col1_x, y + 35);
        setup_interactive_events(slider2);
        artboard->add_child(slider2);

        y += 80;

        // Section: Progress Bars
        add_section_title("Progress Bars", col1_x, y);
        y += 35;

        auto progress1 = flex::create_component_instance("ProgressBar", {
            {"value", 0.25f},
            {"width", 300.0f}
        });
        progress1->set_position(col1_x, y);
        artboard->add_child(progress1);

        auto progress2 = flex::create_component_instance("ProgressBar", {
            {"value", 0.6f},
            {"width", 300.0f}
        });
        progress2->set_position(col1_x, y + 20);
        artboard->add_child(progress2);

        auto progress3 = flex::create_component_instance("ProgressBar", {
            {"value", 1.0f},
            {"width", 300.0f}
        });
        progress3->set_position(col1_x, y + 40);
        artboard->add_child(progress3);

        // ===== Column 2 =====
        y = 110;

        // Section: Text Input
        add_section_title("Text Input", col2_x, y);
        y += 35;

        auto textbox1 = flex::create_component_instance("TextBox", {
            {"placeholder", std::string("Enter your name...")},
            {"width", 300.0f}
        });
        textbox1->set_position(col2_x, y);
        setup_interactive_events(textbox1);
        artboard->add_child(textbox1);

        auto textbox2 = flex::create_component_instance("TextBox", {
            {"text", std::string("Hello, World!")},
            {"width", 300.0f}
        });
        textbox2->set_position(col2_x, y + 45);
        setup_interactive_events(textbox2);
        artboard->add_child(textbox2);

        auto textbox_disabled = flex::create_component_instance("TextBox", {
            {"text", std::string("Disabled")},
            {"width", 300.0f},
            {"enabled", false}
        });
        textbox_disabled->set_position(col2_x, y + 90);
        artboard->add_child(textbox_disabled);

        y += 150;

        // Section: ComboBox
        add_section_title("ComboBox", col2_x, y);
        y += 35;

        auto combo1 = flex::create_component_instance("ComboBox", {
            {"items", std::string("Select an option,Option 1,Option 2,Option 3")},
            {"selectedIndex", 0.0f},
            {"width", 250.0f}
        });
        combo1->set_position(col2_x, y);
        setup_interactive_events(combo1);
        artboard->add_child(combo1);

        auto combo2 = flex::create_component_instance("ComboBox", {
            {"items", std::string("Small,Medium,Large,Extra Large")},
            {"selectedIndex", 1.0f},
            {"width", 250.0f}
        });
        combo2->set_position(col2_x, y + 45);
        setup_interactive_events(combo2);
        artboard->add_child(combo2);

        y += 110;

        // Section: ListBox
        add_section_title("ListBox", col2_x, y);
        y += 35;

        auto listbox = flex::create_component_instance("ListBox", {
            {"items", std::string("Apple,Banana,Cherry,Date,Elderberry")},
            {"selectedIndex", 2.0f},
            {"width", 200.0f},
            {"height", 160.0f}
        });
        listbox->set_position(col2_x, y);
        setup_interactive_events(listbox);
        artboard->add_child(listbox);

        // Section: Labels (next to listbox)
        add_section_title("Labels", col2_x + 230, y - 35);

        auto label1 = flex::create_component_instance("Label", {
            {"text", std::string("Primary Label")},
            {"fontSize", 16.0f}
        });
        label1->set_position(col2_x + 230, y);
        artboard->add_child(label1);

        auto label2 = flex::create_component_instance("Label", {
            {"text", std::string("Secondary text")},
            {"fontSize", 12.0f},
            {"color", 0xFF5D5D5D}
        });
        label2->set_position(col2_x + 230, y + 25);
        artboard->add_child(label2);

        auto label3 = flex::create_component_instance("Label", {
            {"text", std::string("Accent color")},
            {"fontSize", 14.0f},
            {"color", 0xFF0078D4}
        });
        label3->set_position(col2_x + 230, y + 50);
        artboard->add_child(label3);

        // Disabled states section
        y += 200;
        add_section_title("Disabled States", col2_x, y);
        y += 35;

        auto btn_disabled = flex::create_component_instance("Button", {
            {"text", std::string("Disabled")},
            {"enabled", false},
            {"width", 100.0f}
        });
        btn_disabled->set_position(col2_x, y);
        artboard->add_child(btn_disabled);

        auto cb_disabled = flex::create_component_instance("Checkbox", {
            {"text", std::string("Disabled")},
            {"enabled", false}
        });
        cb_disabled->set_position(col2_x + 120, y);
        artboard->add_child(cb_disabled);

        auto slider_disabled = flex::create_component_instance("Slider", {
            {"value", 0.5f},
            {"width", 150.0f},
            {"enabled", false}
        });
        slider_disabled->set_position(col2_x + 250, y);
        artboard->add_child(slider_disabled);
    }

    void add_section_title(const std::string& text, float x, float y) {
        auto title = flex::Text::create();
        title->set_content(text);
        title->set_font_size(16);
        title->set_color(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
        title->set_position(x, y);
        instance_->artboard()->add_child(title);
    }

    // Setup hover effects for buttons
    void setup_button_events(flex::Node::Ptr node, const std::string& name) {
        if (!node) return;

        // Find the background shape for color changes
        auto* bg = node->find("bg");

        node->on_hover_enter([bg, name](flex::PointerEvent&) {
            std::cout << name << " button: hover enter\n";
            if (bg) {
                auto* shape = dynamic_cast<flex::Shape*>(bg);
                if (shape) {
                    // Lighten the background on hover
                    shape->set_fill(flex::Color(0.1f, 0.5f, 0.9f, 1.0f));
                }
            }
        });

        node->on_hover_leave([bg, name](flex::PointerEvent&) {
            std::cout << name << " button: hover leave\n";
            if (bg) {
                auto* shape = dynamic_cast<flex::Shape*>(bg);
                if (shape) {
                    // Restore original color
                    shape->set_fill(flex::Color(0.0f, 0.47f, 0.83f, 1.0f));
                }
            }
        });

        node->on_pointer_down([bg, name](flex::PointerEvent&) {
            std::cout << name << " button: pressed\n";
            if (bg) {
                auto* shape = dynamic_cast<flex::Shape*>(bg);
                if (shape) {
                    // Darken on press
                    shape->set_fill(flex::Color(0.0f, 0.35f, 0.62f, 1.0f));
                }
            }
        });

        node->on_pointer_up([bg, name](flex::PointerEvent&) {
            std::cout << name << " button: released\n";
            if (bg) {
                auto* shape = dynamic_cast<flex::Shape*>(bg);
                if (shape) {
                    shape->set_fill(flex::Color(0.1f, 0.5f, 0.9f, 1.0f));
                }
            }
        });

        node->on_click([name]() {
            std::cout << "*** " << name << " button CLICKED! ***\n";
        });
    }

    // Setup basic hover effect for other interactive widgets
    void setup_interactive_events(flex::Node::Ptr node) {
        if (!node) return;

        node->on_hover_enter([](flex::PointerEvent&) {
            // Could add cursor change here
        });

        node->on_click([]() {
            std::cout << "Widget clicked\n";
        });
    }

    void run() {
        bool running = true;
        SDL_Event event;
        bool mouse_down = false;

        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            mouse_down = true;
                            instance_->send_pointer_event(
                                static_cast<float>(event.button.x),
                                static_cast<float>(event.button.y),
                                true
                            );
                        }
                        break;

                    case SDL_MOUSEBUTTONUP:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            mouse_down = false;
                            instance_->send_pointer_event(
                                static_cast<float>(event.button.x),
                                static_cast<float>(event.button.y),
                                false
                            );
                        }
                        break;

                    case SDL_MOUSEMOTION:
                        instance_->send_pointer_event(
                            static_cast<float>(event.motion.x),
                            static_cast<float>(event.motion.y),
                            mouse_down
                        );
                        break;
                }
            }

            render();
            SDL_Delay(16);  // ~60 FPS
        }
    }

    void render() {
        std::fill(buffer_.begin(), buffer_.end(), 0xFFF3F3F3);  // Light gray

        // Render flex scene
        if (instance_ && flex_renderer_) {
            flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
            instance_->artboard()->render(*flex_renderer_);
            flex_renderer_->end_frame();
        }

        canvas_->draw();
        canvas_->sync();

        // Update SDL texture
        SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

    void cleanup() {
        flex_renderer_.reset();
        instance_.reset();
        canvas_.reset();

        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::vector<uint32_t> buffer_;

    std::unique_ptr<tvg::SwCanvas> canvas_;
    flex::Instance::Ptr instance_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    WidgetGalleryDemo demo;

    if (!demo.init()) {
        std::cerr << "Failed to initialize demo\n";
        return 1;
    }

    demo.run();
    demo.cleanup();

    return 0;
}
