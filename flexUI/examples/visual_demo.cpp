#ifndef WINDOWS_LEAN_AND_MEAN
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h> 
#include <flexUI.h>
#include "glfw_app.h"
#include <flex/bridge/renderer.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/tree_widget.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/image_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/widgets/card_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include "demo_styles.h"

using namespace flexUI;

class VisualDemo : public ::flex::GlfwApp {
public:
    VisualDemo() : ::flex::GlfwApp("flexUI Visual Demo", 1200, 900) {}

protected:
    bool on_init() override {
        // Try multiple paths for the font file
        const char* font_paths[] = {
            "fonts/NotoSansSC-Regular.ttf",
         };

        bool loaded = false;
        for (const char* path : font_paths) {
            if (load_font("NotoSansSC", path)) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            std::cerr << "Warning: Could not load NotoSansSC font." << std::endl;
        }

        if (!load_font("Consolas", "C:/Windows/Fonts/consola.ttf")) {
            std::cerr << "Warning: Could not load Consolas font." << std::endl;
        }

        box_ = std::make_unique<flexUI::Box>(renderer());
        box_->set_viewport((float)width(), (float)height());
        box_->load_css(examples::DEMO_CSS);

        auto* root = box_->create("div", "root");
        box_->set_root(root);

        // Sidebar
        auto* sidebar = box_->create("div", "sidebar");
        root->append(sidebar);

        auto* brand = box_->create("div");
        brand->add_class("brand");
        brand->append(box_->create_widget<LabelWidget>("span", "", "flexUI"));
        sidebar->append(brand);

        const char* menu_items[] = {"Gallery", "Calendar", "Data", "Tabs"};
        for (int i = 0; i < 4; ++i) {
            auto* btn = box_->create_widget<ButtonWidget>("button", "");
            btn->add_class("nav-item");
            static_cast<ButtonWidget*>(btn->widget)->set_text(menu_items[i]);
            if (i == 0) btn->add_class("active");
            
            nav_buttons_[i] = btn;
            sidebar->append(btn);
        }

        sidebar->append(box_->create("divider", ""));

        // Main Content
        auto* main = box_->create("div", "main-content");
        root->append(main);

        // Header
        auto* header = box_->create("div");
        header->style_.display = Display::Flex;
        header->style_.flex_direction = FlexDirection::Row;
        header->style_.justify_content = JustifyContent::SpaceBetween;
        header->style_.align_items = AlignItems::Center;
        header->style_.width = 860.0f;
        header->style_.height = 60.0f;
        header->style_.margin[2] = 20.0f; 

        title_label_ = box_->create_widget<LabelWidget>("label", "", "Component Gallery");
        title_label_->style_.font_size = 24.0f;
        title_label_->style_.width = 400.0f;
        header->append(title_label_);
        
        auto* search = box_->create_widget<InputWidget>("input", "search-box");
        static_cast<InputWidget*>(search->widget)->set_placeholder("Search...");
        header->append(search);
        main->append(header);

        // View Container
        auto* container = box_->create("div", "view-container");
        main->append(container);

        // Build all views
        views_[0] = build_gallery_view();
        views_[1] = build_calendar_view();
        views_[2] = build_data_view();
        views_[3] = build_tabs_view();

        for (int i = 0; i < 4; ++i) {
            container->append(views_[i]);
            if (i > 0) views_[i]->add_class("hidden");
        }

        // Global event handler
        box_->set_event_callback([this](Element& elem, const Event& e) {
            if (e.type == EventType::MouseDown) {
                for (int i = 0; i < 4; ++i) {
                    if (nav_buttons_[i] == &elem) {
                        this->switch_view(i);
                        break;
                    }
                }
            }
        });

        box_->update();
        return true;
    }

    void switch_view(int index) {
        if (index < 0 || index >= 4) return;
        
        static const char* titles[] = {"Component Gallery", "Calendar & Scheduling", "Data Management", "Interface Tabs"};
        static_cast<LabelWidget*>(title_label_->widget)->set_text(titles[index]);

        for (int i = 0; i < 4; ++i) {
            if (views_[i]) {
                if (i == index) views_[i]->remove_class("hidden");
                else views_[i]->add_class("hidden");
            }
            if (nav_buttons_[i]) {
                if (i == index) nav_buttons_[i]->add_class("active");
                else nav_buttons_[i]->remove_class("active");
            }
        }
        
        box_->invalidate();
        box_->update();
    }

    void on_update(float dt) override {
        box_->update_time(dt * 1000.0f);
        progress_val_ += dt * 0.05f;
        if (progress_val_ > 1.0f) progress_val_ = 0.0f;
        if (progress_elem_) {
            static_cast<ProgressBarWidget*>(progress_elem_->widget)->set_value(progress_val_ * 100.0f);
            progress_elem_->mark_paint_dirty();
        }

        // Sync IME position
        box_->update();
        auto* focused = box_->focused_element();
        if (focused && focused->widget && focused->widget->wants_text_input()) {
            float x = 0, y = 0, w = 0, h = 0;
            focused->widget->get_caret_rect(*focused, x, y, w, h);
            // Convert local caret pos to screen pos (physical pixels)
            flex::Vec2 screen_pos = focused->to_world(flex::Vec2(x, y + h));
            update_ime_position((int)screen_pos.x(), (int)screen_pos.y());
        }
    }

    void on_render() override {
        box_->invalidate();
        box_->update();
        canvas()->draw();
        canvas()->sync();
    }

    void on_resize(int w, int h) override {
        ::flex::GlfwApp::on_resize(w, h);
        if (box_) box_->set_viewport((float)w, (float)h);
    }

    void on_mouse_button(int button, int action, int mods) override {
        double x, y;
        glfwGetCursorPos(window(), &x, &y);
        auto e = (action == GLFW_PRESS)
            ? Event::mouse_down((float)x, (float)y, glfw_to_button(button))
            : Event::mouse_up((float)x, (float)y, glfw_to_button(button));
        box_->dispatch_event(e);
    }

    void on_cursor_pos(double x, double y) override {
        auto e = Event::mouse_move((float)x, (float)y);
        box_->dispatch_event(e);
    }

    void on_key(int key, int action, int mods) override {
        if (action == GLFW_PRESS) {
            if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
                switch_view(key - GLFW_KEY_1);
            }
        }
        auto e = (action == GLFW_PRESS || action == GLFW_REPEAT)
            ? Event::key_down(glfw_to_keycode(key), glfw_to_mods(mods))
            : Event::key_up(glfw_to_keycode(key), glfw_to_mods(mods));
        box_->dispatch_event(e);
    }

    void on_scroll(double dx, double dy) override {
        double x, y;
        glfwGetCursorPos(window(), &x, &y);
        auto e = flexUI::Event::mouse_wheel((float)x, (float)y, (float)dx, (float)dy);
        box_->dispatch_event(e);
    }

    void on_char(unsigned int codepoint) override {
        std::string utf8;
        if (codepoint < 0x80) utf8 += (char)codepoint;
        else if (codepoint < 0x800) {
            utf8 += (char)(0xc0 | (codepoint >> 6));
            utf8 += (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint < 0x10000) {
            utf8 += (char)(0xe0 | (codepoint >> 12));
            utf8 += (char)(0x80 | ((codepoint >> 6) & 0x3f));
            utf8 += (char)(0x80 | (codepoint & 0x3f));
        }
        auto e = Event::text_input(utf8);
        box_->dispatch_event(e);
    }

    void on_composition_start() override {
        auto e = Event::composition_start();
        box_->dispatch_event(e);
    }

    void on_composition_update(const std::string& text) override {
        auto e = Event::composition_update(text);
        box_->dispatch_event(e);
    }

    void on_composition_end() override {
        auto e = Event::composition_end();
        box_->dispatch_event(e);
    }

private:
    Element* build_gallery_view() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        view->add_class("gallery-grid");
        
        auto* col1 = box_->create("div");
        col1->add_class("col");
        view->append(col1);

        auto* card1 = box_->create("div");
        card1->add_class("card");
        auto* t1 = box_->create("div"); t1->add_class("card-title");
        t1->append(box_->create_widget<LabelWidget>("span", "", "UI Components"));
        card1->append(t1);

        auto* r1 = box_->create("div"); r1->add_class("card-row");
        r1->append(box_->create_widget<ButtonWidget>("button", "", "Primary"));
        auto* g = box_->create_widget<ButtonWidget>("button", "", "Secondary");
        g->add_class("ghost");
        r1->append(g);
        card1->append(r1);

        auto* r2 = box_->create("div"); r2->add_class("card-row");
        r2->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Push Status", true));
        r2->append(box_->create_widget<SwitchWidget>("switch", "", "Power", true));
        card1->append(r2);
        
        auto* r3 = box_->create("div"); r3->add_class("card-row");
        auto* text_input = box_->create_widget<TextAreaWidget>("div", "");
        auto* taw = static_cast<TextAreaWidget*>(text_input->widget);
        taw->set_placeholder("Type with IME here...");
        text_input->style_.width = 360.0f;
        text_input->style_.height = 80.0f;
        r3->append(text_input);
        card1->append(r3);

        col1->append(card1);

        auto* col2 = box_->create("div");
        col2->add_class("col");
        view->append(col2);

        auto* card2 = box_->create("div");
        card2->add_class("card");
        auto* t2 = box_->create("div"); t2->add_class("card-title");
        t2->append(box_->create_widget<LabelWidget>("span", "", "System Metrics"));
        card2->append(t2);

        progress_elem_ = box_->create_widget<ProgressBarWidget>("progressbar", "");
        static_cast<ProgressBarWidget*>(progress_elem_->widget)->set_value(65.0f);
        card2->append(progress_elem_);

        auto* badges = box_->create("div");
        badges->add_class("card-row");
        auto* b1 = box_->create_widget<BadgeWidget>("badge", "", "System OK"); b1->add_class("success");
        badges->append(b1);
        auto* b2 = box_->create_widget<BadgeWidget>("badge", "", "High Load"); b2->add_class("danger");
        badges->append(b2);
        card2->append(badges);
        col2->append(card2);

        return view;
    }

    Element* build_calendar_view() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        view->append(box_->create_widget<LabelWidget>("h2", "", "Monthly Schedule"));
        view->append(box_->create_widget<CalendarWidget>("calendar", ""));
        return view;
    }

    Element* build_data_view() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        view->append(box_->create_widget<LabelWidget>("h2", "", "User Database"));
        auto* table = box_->create_widget<TableWidget>("table", "");
        auto* tw = static_cast<TableWidget*>(table->widget);
        tw->add_column("ID", 60.0f);
        tw->add_column("Name", 200.0f);
        tw->add_column("Department", 150.0f);
        tw->add_column("Status", 120.0f);
        
        tw->add_row({"001", "Alice Smith", "Engineering", "Active"});
        tw->add_row({"002", "Bob Jones", "Design", "On Leave"});
        tw->add_row({"003", "Charlie Brown", "Product", "Active"});
        tw->add_row({"004", "Diana Prince", "Security", "Active"});
        tw->add_row({"005", "Edward Norton", "HR", "Active"});
        
        view->append(table);
        return view;
    }

    Element* build_tabs_view() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        view->add_class("tabs-view");
        
        auto* tabs_elem = box_->create_widget<TabsWidget>("tabs", "");
        auto* tw = static_cast<TabsWidget*>(tabs_elem->widget);
        
        // Page 1 - Selectable Text using TextAreaWidget (Read-Only)
        auto* page1 = box_->create("div", "tab-page-1");
        page1->add_class("tab-page");
        page1->append(box_->create_widget<LabelWidget>("h2", "", "Selectable Content"));
        
        auto* selectable_text = box_->create_widget<TextAreaWidget>("div", "");
        auto* taw = static_cast<TextAreaWidget*>(selectable_text->widget);
        taw->set_text("This text is rendered using a TextAreaWidget set to read-only mode.\n\n"
                      "Unlike static labels, you can click and drag to SELECT this text.\n"
                      "This demonstrates how to achieve selectable content in flexUI.");
        taw->set_readonly(true);
        selectable_text->style_.width = 820.0f;
        selectable_text->style_.height = 300.0f;
        // Adjust background to look like a normal page area
        selectable_text->style_.background_color = Color(1,1,1,0); 
        selectable_text->style_.variables[Symbol("--textarea-bg")] = "0,0,0,0";
        page1->append(selectable_text);
        
        // Page 2
        auto* page2 = box_->create("div", "tab-page-2");
        page2->add_class("tab-page");
        page2->add_class("hidden");
        page2->append(box_->create_widget<LabelWidget>("h2", "", "Advanced Components"));

        // Page 3
        auto* page3 = box_->create("div", "tab-page-3");
        page3->add_class("tab-page");
        page3->add_class("hidden");
        page3->append(box_->create_widget<LabelWidget>("h2", "", "Network Config"));
        page3->append(box_->create_widget<LabelWidget>("p", "", "Manage connections and proxy settings."));

        tw->add_tab("Selectable Text", "gen", page1);
        tw->add_tab("Advanced", "adv", page2);
        tw->add_tab("Network", "net", page3);

        view->append(tabs_elem);
        
        auto* content = box_->create("div", "tab-content");
        content->add_class("tab-content");
        content->append(page1);
        content->append(page2);
        content->append(page3);
        view->append(content);

        return view;
    }

    std::unique_ptr<Box> box_;
    float progress_val_ = 0.0f;
    Element* progress_elem_ = nullptr;
    Element* title_label_ = nullptr;
    Element* nav_buttons_[4] = {nullptr};
    Element* views_[4] = {nullptr};
};

int main() {
    VisualDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
