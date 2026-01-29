/*
 * flexUI TUI Demo
 *
 * Demonstrates flexUI widgets rendering to terminal using TUI backend.
 */

#include <tui.h>
#include <flex/backends/tui/init.h>
#include <flexUI/box.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/draggable_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/virtualized_list_widget.h>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace flexUI;

// Simple CSS for TUI (minimal styling)
static const char* TUI_CSS = R"(
.root {
    padding: 16px;
}
.card {
    padding: 8px;
    margin: 8px;
}
.card-title {
    font-size: 14px;
    margin-bottom: 8px;
}
.card-row {
    display: flex;
    flex-direction: row;
    gap: 16px;
    margin: 8px 0;
}
.col {
    display: flex;
    flex-direction: column;
}
.draggable {
    background-color: #2a2a2a;
    border: 1px solid #0078d4;
    padding: 8px;
    z-index: 100;
}
.drag-title {
    color: #ffffff;
    font-weight: bold;
}
)";

class TuiDemo {
public:
    bool init() {
        // Initialize terminal
        if (!term_.init()) {
            return false;
        }
        
        // Create TUI renderer for flex
        renderer_ = flex::tui_backend::create_renderer(term_);
        if (!renderer_) {
            return false;
        }
        
        // Create flexUI Box
        box_ = std::make_unique<Box>(renderer_.get());
        
        // Set viewport based on terminal size (convert chars to pixels)
        float vp_w = static_cast<float>(term_.width() * flex::tui_backend::CHAR_WIDTH);
        float vp_h = static_cast<float>(term_.height() * flex::tui_backend::CHAR_HEIGHT);
        box_->set_viewport(vp_w, vp_h);
        
        // Load CSS
        box_->load_css(TUI_CSS);
        
        // Enable mouse
        input_.enable_mouse();
        
        // Build UI
        build_ui();
        
        return true;
    }
    
    void run() {
        running_ = true;
        tui::Input input;
        
        while (running_) {
            // DRAIN INPUT: Process all pending events to avoid lag
            while (auto event = input_.poll()) {
                if (event->is(tui::Key::Escape) || event->is('q')) {
                    running_ = false;
                    break;
                }
                
                if (event->is_ctrl('r')) {
                    term_.refresh();
                    continue;
                }

                if (event->type == tui::Event::Resize) {
                     term_.query_size();
                     float vp_w = static_cast<float>(term_.width() * flex::tui_backend::CHAR_WIDTH);
                     float vp_h = static_cast<float>(term_.height() * flex::tui_backend::CHAR_HEIGHT);
                     box_->set_viewport(vp_w, vp_h);
                     term_.refresh();
                     continue;
                }

                // Map TUI event to flexUI event
                Event fe;
                fe.x = static_cast<float>(event->x * flex::tui_backend::CHAR_WIDTH + flex::tui_backend::CHAR_WIDTH / 2.0f);
                fe.y = static_cast<float>(event->y * flex::tui_backend::CHAR_HEIGHT + flex::tui_backend::CHAR_HEIGHT / 2.0f);

                if (event->type == tui::Event::MousePress) {
                    if (event->key == tui::Key::MouseWheelUp) {
                        fe.type = EventType::MouseWheel;
                        fe.delta_y = 10.0f; // Scroll speed
                        box_->dispatch_event(fe);
                    } else if (event->key == tui::Key::MouseWheelDown) {
                        fe.type = EventType::MouseWheel;
                        fe.delta_y = -10.0f;
                        box_->dispatch_event(fe);
                    } else {
                        fe.type = EventType::MouseDown;
                        if (event->key == tui::Key::MouseLeft) fe.button = MouseButton::Left;
                        else if (event->key == tui::Key::MouseRight) fe.button = MouseButton::Right;
                        else if (event->key == tui::Key::MouseMiddle) fe.button = MouseButton::Middle;
                        box_->dispatch_event(fe);
                    }
                } else if (event->type == tui::Event::MouseRelease) {
                    fe.type = EventType::MouseUp;
                    box_->dispatch_event(fe);
                } else if (event->type == tui::Event::MouseMove) {
                    fe.type = EventType::MouseMove;
                    box_->dispatch_event(fe);
                }
            }
            
            if (!running_) break;

            // Update animation
            progress_value_ += 0.005f; // Slower progress for 60fps
            if (progress_value_ > 1.0f) progress_value_ = 0.0f;
            
            if (progress_elem_) {
                auto* pw = static_cast<ProgressBarWidget*>(progress_elem_->widget);
                if (pw) {
                    pw->set_value(progress_value_ * 100.0f);
                    progress_elem_->mark_paint_dirty();
                }
            }
            
            // Update and render
            box_->update_time(16.0f); // 16ms = ~60fps
            box_->update();
            
            // Render to terminal
            term_.render();
            
            // Sleep to avoid CPU spinning (16ms = 60fps)
#ifdef _WIN32
            Sleep(16);
#else
            usleep(16000);
#endif
        }
    }
    
    ~TuiDemo() {
        box_.reset();
        renderer_.reset();
        term_.cleanup();
    }

private:
    tui::Terminal term_;
    tui::Input input_;
    std::unique_ptr<flex::Renderer> renderer_;
    std::unique_ptr<Box> box_;
    bool running_ = false;
    
    // UI elements
    Element* progress_elem_ = nullptr;
    float progress_value_ = 0.0f;
    
    void build_ui() {
        auto* root = box_->create("div", "root");
        root->add_class("root");
        box_->set_root(root);
        
        // Title with ANSI colors
        auto* title = box_->create_widget<LabelWidget>("label", "title", "\x1b[1;31mf\x1b[32ml\x1b[33me\x1b[34mx\x1b[35mUI\x1b[0m \x1b[1mTerminal Demo\x1b[0m");
        title->add_class("card-title");
        root->append(title);
        
        // Divider
        auto* div1 = box_->create_widget<DividerWidget>("divider", "", DividerWidget::Orientation::Horizontal);
        root->append(div1);
        
        // Buttons row
        auto* btn_row = box_->create("div", "btn-row");
        btn_row->add_class("card-row");
        root->append(btn_row);
        
        btn_row->append(box_->create_widget<ButtonWidget>("button", "", "Button 1"));
        btn_row->append(box_->create_widget<ButtonWidget>("button", "", "Button 2"));
        btn_row->append(box_->create_widget<ButtonWidget>("button", "", "Exit (Q)"));
        
        // Log Viewer (Virtualized List Demo)
        auto* log_card = box_->create("div", "log-card");
        log_card->add_class("card");
        log_card->computed_style->height = 128; // Fixed height: 8 lines * 16px
        log_card->computed_style->margin[2] = 16; // margin-bottom
        log_card->computed_style->border_width[0] = 1;
        log_card->computed_style->border_width[1] = 1;
        log_card->computed_style->border_width[2] = 1;
        log_card->computed_style->border_width[3] = 1;
        log_card->computed_style->border_color = {0.3f, 0.3f, 0.3f, 1.0f};
        log_card->set_clip(true);
        
        auto* vlist_widget = new VirtualizedListWidget();
        auto* vlist = box_->create_with_widget("div", vlist_widget, "vlist");
        vlist->computed_style->position = Position::Absolute;
        vlist->computed_style->left = 0; vlist->computed_style->top = 0;
        vlist->computed_style->right = 0; vlist->computed_style->bottom = 0;
        
        vlist_widget->set_item_count(10000);
        vlist_widget->set_row_height(16); // 16px = 1 char height

        vlist_widget->set_create_row_fn([this]() {
            auto* row = box_->create("div", "");
            auto* label = box_->create_widget<LabelWidget>("label", "", "");
            row->append(label);
            return row;
        });

        vlist_widget->set_bind_row_fn([](Element* row, int index) {
            if (row->child_count() > 0) {
                auto* label = row->child_at(0)->widget_as<LabelWidget>();
                if (label) {
                    char buf[64];
                    if (index % 5 == 0) snprintf(buf, 64, "\x1b[31m[ERROR]\x1b[0m System failure at index %d", index);
                    else if (index % 3 == 0) snprintf(buf, 64, "\x1b[33m[WARN]\x1b[0m High latency detected %dms", index % 100);
                    else snprintf(buf, 64, "\x1b[32m[INFO]\x1b[0m Processed request ID #%08X", index);
                    label->set_text(buf);
                }
            }
        });

        log_card->append(vlist);
        root->append(log_card);

        // Controls row
        auto* ctrl_row = box_->create("div", "ctrl-row");
        ctrl_row->add_class("card-row");
        root->append(ctrl_row);
        
        ctrl_row->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Option A", true));
        ctrl_row->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Option B", false));
        ctrl_row->append(box_->create_widget<SwitchWidget>("switch", "", "Toggle", true));
        
        // Progress bar
        auto* prog_label = box_->create_widget<LabelWidget>("label", "", "Progress:");
        root->append(prog_label);
        
        progress_elem_ = box_->create_widget<ProgressBarWidget>("progressbar", "prog", 0.0f, false);
        root->append(progress_elem_);

        // Slider (Interactive D&D-like control)
        auto* slider_label = box_->create_widget<LabelWidget>("label", "", "Interactive Slider (Drag me):");
        root->append(slider_label);
        
        auto* slider = box_->create_widget<SliderWidget>("slider", "v-slider", 0.0f, 100.0f, 50.0f);
        root->append(slider);

        // Input Field (Mouse edit support)
        auto* input_label = box_->create_widget<LabelWidget>("label", "", "Edit Me (Click & Drag to select):");
        root->append(input_label);

        auto* input = box_->create_widget<InputWidget>("input", "v-input", "Type something...");
        static_cast<InputWidget*>(input->widget)->set_text("D&D demo text");
        root->append(input);

        // ANSI 24-bit color demo
        auto* ansi_demo = box_->create_widget<LabelWidget>("label", "", "ANSI 24-bit: \x1b[38;2;255;100;0mOrange\x1b[0m \x1b[38;2;0;255;128mSpringGreen\x1b[0m");
        root->append(ansi_demo);

        // Wide Character Demo
        auto* wide_demo = box_->create_widget<LabelWidget>("label", "", "Wide: [汉字] [👋] [Mixed 🆗]");
        root->append(wide_demo);
        
        // Badge
        auto* badge_row = box_->create("div", "badge-row");
        badge_row->add_class("card-row");
        root->append(badge_row);
        
        auto* badge = box_->create_widget<BadgeWidget>("badge", "", "");
        static_cast<BadgeWidget*>(badge->widget)->set_count(42);
        badge_row->append(badge);
        
        badge_row->append(box_->create_widget<LabelWidget>("label", "", "Notifications"));
        
        // Footer
        auto* div2 = box_->create_widget<DividerWidget>("divider", "", DividerWidget::Orientation::Horizontal);
        root->append(div2);
        
        auto* footer = box_->create_widget<LabelWidget>("label", "", "Press Q or ESC to quit");
        root->append(footer);

        // Draggable Panel
        auto* drag_panel = box_->create_with_widget("div", new DraggableWidget(), "drag-panel");
        drag_panel->add_class("draggable");
        drag_panel->computed_style->position = Position::Absolute;
        drag_panel->computed_style->left = 400;
        drag_panel->computed_style->top = 100;
        drag_panel->computed_style->width = 160;
        drag_panel->computed_style->height = 80;
        root->append(drag_panel);

        auto* drag_label = box_->create_widget<LabelWidget>("label", "", "[*] Drag Me!");
        drag_label->add_class("drag-title");
        drag_panel->append(drag_label);

        auto* drag_desc = box_->create_widget<LabelWidget>("label", "", "TUI Drag Demo");
        drag_panel->append(drag_desc);
    }
};

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    TuiDemo demo;
    
    if (!demo.init()) {
        return 1;
    }
    
    demo.run();
    
    return 0;
}
