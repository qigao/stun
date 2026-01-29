/*
 * flexUI - Settings Panel Demo
 *
 * Demonstrates new widgets: Sidebar, Toolbar, ListView, SearchBox,
 * DatePicker, TimePicker, Dialog, Notification, Menu, ScrollView
 */

#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/bridge/renderer.h>
#include <flexUI/box.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/toolbar_widget.h>
#include <flexUI/widgets/listview_widget.h>
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/datepicker_widget.h>
#include <flexUI/widgets/timepicker_widget.h>
#include <flexUI/widgets/dialog_widget.h>
#include <flexUI/widgets/notification_widget.h>
#include <flexUI/widgets/menu_widget.h>
#include <flexUI/widgets/scrollview_widget.h>
#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include <unordered_map>

using namespace flexUI;

constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 800;

static const char* SETTINGS_CSS = R"(
.root { display: flex; flex-direction: row; }
.sidebar { width: 220px; height: 100%; }
.main { flex: 1; display: flex; flex-direction: column; background-color: #1e1e1e; }
.toolbar { height: 48px; width: 100%; }
.content { flex: 1; padding: 24px; overflow: auto; background-color: #1e1e1e; }
.page { display: block; }
.hidden { display: none; }
.section { margin-bottom: 24px; }
.section-title { font-size: 18px; margin-bottom: 12px; color: #ffffff; }
.row { display: flex; flex-direction: row; align-items: center; gap: 16px; margin: 8px 0; }
.label { width: 120px; color: #cccccc; }
.input { width: 240px; height: 36px; }
.list { width: 100%; height: 200px; }
.search { width: 300px; height: 36px; }
)";

class SettingsDemo {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;

        window_ = SDL_CreateWindow("flexUI Settings Demo", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window_) return false;

        surface_ = SDL_GetWindowSurface(window_);
        if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

        load_font("Arial", "C:/Windows/Fonts/arial.ttf");

        canvas_.reset(tvg::SwCanvas::gen());
        canvas_->target(static_cast<uint32_t*>(surface_->pixels),
                       surface_->w, surface_->pitch / 4, surface_->h,
                       tvg::ColorSpace::ARGB8888);

        renderer_ = flex::create_thorvg_renderer(canvas_.get());
        box_ = std::make_unique<Box>(renderer_.get());
        box_->set_viewport(WINDOW_WIDTH, WINDOW_HEIGHT);
        box_->load_css(SETTINGS_CSS);

        build_ui();
        return true;
    }

    void run() {
        running_ = true;
        Uint32 last = SDL_GetTicks();

        while (running_) {
            Uint32 now = SDL_GetTicks();
            float delta = static_cast<float>(now - last);
            last = now;

            handle_events();
            box_->update_time(delta);
            box_->update();
            SDL_UpdateWindowSurface(window_);
            SDL_Delay(16);
        }
    }

    ~SettingsDemo() {
        box_.reset();
        renderer_.reset();
        canvas_.reset();
        tvg::Initializer::term();
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::unique_ptr<flex::Renderer> renderer_;
    std::unique_ptr<Box> box_;
    bool running_ = false;

    // UI elements
    Element* notification_elem_ = nullptr;
    Element* dialog_elem_ = nullptr;
    Element* menu_elem_ = nullptr;
    Element* content_ = nullptr;
    std::unordered_map<std::string, Element*> pages_;
    std::string current_page_ = "general";

    void build_ui() {
        auto* root = box_->create("div", "root");
        root->add_class("root");
        box_->set_root(root);

        // Sidebar
        auto* sidebar_elem = box_->create_widget<SidebarWidget>("div", "sidebar", true);
        sidebar_elem->add_class("sidebar");
        auto* sidebar = static_cast<SidebarWidget*>(sidebar_elem->widget);
        
        sidebar->add_section("Settings");
        sidebar->add_item("general", "*", "General");
        sidebar->add_item("account", "*", "Account");
        sidebar->add_item("notifications", "*", "Notifications", "3");
        sidebar->add_item("privacy", "*", "Privacy");
        sidebar->add_item("appearance", "*", "Appearance");
        
        sidebar->add_section("Advanced");
        sidebar->add_item("network", "*", "Network");
        sidebar->add_item("storage", "*", "Storage");
        sidebar->add_item("about", "*", "About");
        
        sidebar->select("general");
        sidebar->on_select([this](const std::string& id) {
            switch_page(id);
        });
        root->append(sidebar_elem);

        // Main area
        auto* main = box_->create("div", "main");
        main->add_class("main");
        root->append(main);

        // Toolbar
        auto* toolbar_elem = box_->create_widget<ToolbarWidget>("div", "toolbar");
        toolbar_elem->add_class("toolbar");
        auto* toolbar = static_cast<ToolbarWidget*>(toolbar_elem->widget);
        
        toolbar->add_button("save", ">", [this]() { 
            show_notification("Saved", "Settings saved successfully", NotificationWidget::Type::Success);
        }, "Save");
        toolbar->add_button("undo", "<", nullptr, "Undo");
        toolbar->add_button("redo", ">", nullptr, "Redo");
        toolbar->add_separator();
        toolbar->add_toggle("dark", "D", false, "Dark Mode");
        toolbar->add_separator();
        toolbar->add_dropdown("export", "E", {{"json", "Export JSON"}, {"xml", "Export XML"}}, "Export");
        
        toolbar->on_toggle([this](const std::string& id, bool v) {
            if (id == "dark") {
                show_notification("Theme", v ? "Dark mode enabled" : "Light mode enabled", NotificationWidget::Type::Info);
            }
        });
        main->append(toolbar_elem);

        // Content
        content_ = box_->create("div", "content");
        content_->add_class("content");
        main->append(content_);

        // Build all pages
        build_page_general();
        build_page_account();
        build_page_notifications();
        build_page_privacy();
        build_page_appearance();
        build_page_network();
        build_page_storage();
        build_page_about();

        // Show initial page
        switch_page("general");

        // Overlays
        build_overlays(root);
    }

    void switch_page(const std::string& id) {
        for (auto& [page_id, elem] : pages_) {
            if (page_id == id) {
                elem->remove_class("hidden");
            } else {
                elem->add_class("hidden");
            }
        }
        current_page_ = id;
    }

    Element* create_page(const std::string& id, const std::string& title) {
        auto* page = box_->create("div", id + "-page");
        page->add_class("page");
        page->add_class("hidden");  // Start hidden, switch_page will show the active one
        content_->append(page);
        pages_[id] = page;

        auto* title_elem = box_->create_widget<LabelWidget>("label", "", title);
        title_elem->add_class("section-title");
        page->append(title_elem);

        return page;
    }

    void build_page_general() {
        auto* page = create_page("general", "General Settings");

        // Search
        auto* search_row = box_->create("div", "");
        search_row->add_class("row");
        page->append(search_row);

        auto* search = box_->create_widget<SearchBoxWidget>("div", "", "Search settings...");
        search->add_class("search");
        auto* sw = static_cast<SearchBoxWidget*>(search->widget);
        sw->add_suggestion("theme", "Theme", "Change appearance");
        sw->add_suggestion("lang", "Language", "Change language");
        sw->add_suggestion("notify", "Notifications", "Notification settings");
        sw->on_select([this](const SearchBoxWidget::Suggestion& s) {
            show_notification("Search", "Selected: " + s.text, NotificationWidget::Type::Info);
        });
        search_row->append(search);

        // Switches
        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Auto-save"));
        row1->append(box_->create_widget<SwitchWidget>("switch", "", "", true));

        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        row2->append(box_->create_widget<LabelWidget>("label", "", "Show tips"));
        row2->append(box_->create_widget<SwitchWidget>("switch", "", "", false));

        // Slider
        auto* row3 = box_->create("div", "");
        row3->add_class("row");
        page->append(row3);
        row3->append(box_->create_widget<LabelWidget>("label", "", "Volume"));
        row3->append(box_->create_widget<SliderWidget>("slider", "", 0, 100, 75));
    }

    void build_page_account() {
        auto* page = create_page("account", "Account Settings");

        // Date picker
        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Birth Date"));
        row1->append(box_->create_widget<DatePickerWidget>("div", "", Date{1990, 1, 15}));

        // Time picker
        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        row2->append(box_->create_widget<LabelWidget>("label", "", "Reminder"));
        row2->append(box_->create_widget<TimePickerWidget>("div", "", Time{9, 0, 0}));

        // Button to show dialog
        auto* row3 = box_->create("div", "");
        row3->add_class("row");
        page->append(row3);
        auto* btn = box_->create_widget<ButtonWidget>("button", "", "Delete Account");
        btn->on_click([this]() { show_dialog(); });
        row3->append(btn);

        // Device list
        auto* list_title = box_->create_widget<LabelWidget>("label", "", "Connected Devices");
        list_title->add_class("section-title");
        page->append(list_title);

        auto* list_elem = box_->create_widget<ListViewWidget>("div", "devices");
        list_elem->add_class("list");
        auto* list = static_cast<ListViewWidget*>(list_elem->widget);
        list->add_item("1", "MacBook Pro", "Last active: Today");
        list->add_item("2", "iPhone 15", "Last active: Yesterday");
        list->add_item("3", "iPad Air", "Last active: 3 days ago");
        list->on_select([this](int, const ListViewWidget::Item& item) {
            show_notification("Device", "Selected: " + item.text, NotificationWidget::Type::Info);
        });
        page->append(list_elem);
    }

    void build_page_notifications() {
        auto* page = create_page("notifications", "Notification Settings");

        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Push notifications"));
        row1->append(box_->create_widget<SwitchWidget>("switch", "", "", true));

        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        row2->append(box_->create_widget<LabelWidget>("label", "", "Email alerts"));
        row2->append(box_->create_widget<SwitchWidget>("switch", "", "", false));

        auto* row3 = box_->create("div", "");
        row3->add_class("row");
        page->append(row3);
        row3->append(box_->create_widget<LabelWidget>("label", "", "Sound"));
        row3->append(box_->create_widget<SwitchWidget>("switch", "", "", true));
    }

    void build_page_privacy() {
        auto* page = create_page("privacy", "Privacy Settings");

        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Share analytics"));
        row1->append(box_->create_widget<SwitchWidget>("switch", "", "", false));

        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        row2->append(box_->create_widget<LabelWidget>("label", "", "Location access"));
        row2->append(box_->create_widget<SwitchWidget>("switch", "", "", false));
    }

    void build_page_appearance() {
        auto* page = create_page("appearance", "Appearance Settings");

        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Dark mode"));
        row1->append(box_->create_widget<SwitchWidget>("switch", "", "", false));

        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        row2->append(box_->create_widget<LabelWidget>("label", "", "Font size"));
        row2->append(box_->create_widget<SliderWidget>("slider", "", 12, 24, 16));
    }

    void build_page_network() {
        auto* page = create_page("network", "Network Settings");

        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Use proxy"));
        row1->append(box_->create_widget<SwitchWidget>("switch", "", "", false));

        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        row2->append(box_->create_widget<LabelWidget>("label", "", "Proxy host"));
        row2->append(box_->create_widget<InputWidget>("input", "", "127.0.0.1"));
    }

    void build_page_storage() {
        auto* page = create_page("storage", "Storage Settings");

        auto* row1 = box_->create("div", "");
        row1->add_class("row");
        page->append(row1);
        row1->append(box_->create_widget<LabelWidget>("label", "", "Cache size"));
        row1->append(box_->create_widget<SliderWidget>("slider", "", 0, 1000, 256));

        auto* row2 = box_->create("div", "");
        row2->add_class("row");
        page->append(row2);
        auto* btn = box_->create_widget<ButtonWidget>("button", "", "Clear Cache");
        btn->on_click([this]() {
            show_notification("Storage", "Cache cleared", NotificationWidget::Type::Success);
        });
        row2->append(btn);
    }

    void build_page_about() {
        auto* page = create_page("about", "About");

        page->append(box_->create_widget<LabelWidget>("label", "", "flexUI Settings Demo"));
        page->append(box_->create_widget<LabelWidget>("label", "", "Version 1.0.0"));
        page->append(box_->create_widget<LabelWidget>("label", "", "Built with flexUI widget library"));
    }

    void build_overlays(Element* root) {
        // Notification manager
        notification_elem_ = box_->create_widget<NotificationWidget>("div", "notifications", 
            NotificationWidget::Position::TopRight);
        root->append(notification_elem_);

        // Dialog
        dialog_elem_ = box_->create_widget<DialogWidget>("div", "dialog", "Confirm Delete", 
            DialogWidget::Type::Warning);
        auto* dialog = static_cast<DialogWidget*>(dialog_elem_->widget);
        dialog->set_message("Are you sure you want to delete your account? This action cannot be undone.");
        dialog->on_action([this](const std::string& id) {
            if (id == "ok") {
                show_notification("Account", "Account deletion requested", NotificationWidget::Type::Error);
            }
        });
        root->append(dialog_elem_);

        // Context menu
        menu_elem_ = box_->create_widget<MenuWidget>("div", "menu");
        auto* menu = static_cast<MenuWidget*>(menu_elem_->widget);
        menu->add_item("cut", "Cut", nullptr, "Ctrl+X");
        menu->add_item("copy", "Copy", nullptr, "Ctrl+C");
        menu->add_item("paste", "Paste", nullptr, "Ctrl+V");
        menu->add_separator();
        menu->add_item("delete", "Delete", nullptr, "Del");
        root->append(menu_elem_);
    }

    void show_notification(const std::string& title, const std::string& msg, NotificationWidget::Type type) {
        if (notification_elem_) {
            static_cast<NotificationWidget*>(notification_elem_->widget)->notify(title, msg, type);
            notification_elem_->mark_paint_dirty();
        }
    }

    void show_dialog() {
        if (dialog_elem_) {
            static_cast<DialogWidget*>(dialog_elem_->widget)->show();
            dialog_elem_->mark_paint_dirty();
        }
    }

    void show_menu(float x, float y) {
        if (menu_elem_) {
            static_cast<MenuWidget*>(menu_elem_->widget)->show(x, y);
            menu_elem_->mark_paint_dirty();
        }
    }

    bool load_font(const char* name, const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) return false;
        return tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) == tvg::Result::Success;
    }

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_MOUSEMOTION: {
                    auto e = Event::mouse_move(event.motion.x, event.motion.y);
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_MOUSEBUTTONDOWN: {
                    MouseButton btn = event.button.button == SDL_BUTTON_LEFT ? MouseButton::Left :
                                     event.button.button == SDL_BUTTON_RIGHT ? MouseButton::Right : MouseButton::Middle;
                    
                    // Right click shows context menu
                    if (btn == MouseButton::Right) {
                        show_menu(event.button.x, event.button.y);
                    }
                    
                    auto e = Event::mouse_down(event.button.x, event.button.y, btn);
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_MOUSEBUTTONUP: {
                    MouseButton btn = event.button.button == SDL_BUTTON_LEFT ? MouseButton::Left :
                                     event.button.button == SDL_BUTTON_RIGHT ? MouseButton::Right : MouseButton::Middle;
                    auto e = Event::mouse_up(event.button.x, event.button.y, btn);
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_MOUSEWHEEL: {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    auto e = Event::mouse_wheel(mx, my, event.wheel.x, event.wheel.y);
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_KEYDOWN: {
                    if (event.key.keysym.sym == SDLK_ESCAPE) running_ = false;
                    break;
                }

                case SDL_TEXTINPUT: {
                    auto e = Event::text_input(event.text.text);
                    box_->dispatch_event(e);
                    break;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    SettingsDemo demo;
    if (!demo.init()) {
        std::cerr << "Failed to initialize" << std::endl;
        return 1;
    }
    demo.run();
    return 0;
}
